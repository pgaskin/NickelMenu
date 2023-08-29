package main

import (
	"archive/tar"
	"bufio"
	"bytes"
	"fmt"
	"io"
	"io/ioutil"
	"net/http"
	"os"
	"path/filepath"
	"sort"
	"strconv"
	"strings"

	"github.com/pgaskin/kobopatch/patchlib"
	"github.com/xi2/xz"
)

func main() {
	sc, err := FindSymChecks(".")
	if err != nil {
		fmt.Fprintf(os.Stderr, "[FTL] find symbol checks: %v\n", err)
		os.Exit(1)
		return
	}

	versions := []string{
		"4.6.9960", "4.6.9995", "4.7.10075", "4.7.10364", "4.7.10413",
		"4.8.10956", "4.8.11073", "4.8.11090", "4.9.11311", "4.9.11314",
		"4.10.11591", "4.10.11655", "4.11.11911", "4.11.11976", "4.11.11980",
		"4.11.11982", "4.11.12019", "4.12.12111", "4.13.12638", "4.14.12777",
		"4.15.12920", "4.16.13162", "4.17.13651", "4.17.13694", "4.18.13737",
		"4.19.14123", "4.20.14601", "4.20.14617", "4.20.14622", "4.21.15015",
		"4.22.15190", "4.22.15268", "4.23.15505", "4.24.15672", "4.24.15676",
		"4.25.15875", "4.26.16704", "4.28.17623", "4.28.17820", "4.28.17826",
		"4.28.17925", "4.28.18220", "4.29.18730", "4.30.18838", "4.31.19086",
		"4.32.19501", "4.33.19608", "4.33.19611", "4.33.19759", "4.34.20097",
		"4.35.20400", "4.36.21095",
	}

	checks := map[string]map[string][]SymCheck{}
	for _, c := range sc {
		var sm, em int
		for _, version := range versions {
			if c.StartVersion == "*" || strings.HasPrefix(version+".", c.StartVersion+".") {
				sm++
			}
			if c.EndVersion == "*" || strings.HasPrefix(version+".", c.EndVersion+".") {
				em++
			}
			if versioncmp(c.StartVersion, version) <= 0 && versioncmp(version, c.EndVersion) <= 0 {
				if _, ok := checks[version]; !ok {
					checks[version] = map[string][]SymCheck{}
				}
				checks[version][c.Library] = append(checks[version][c.Library], c)
			}
		}
		if sm == 0 {
			fmt.Printf("[WRN] %s: no exact match for the base version in specifier %#v\n", c.File, c.StartVersion)
		}
		if em == 0 {
			fmt.Printf("[WRN] %s: no exact match for the base version in specifier %#v\n", c.File, c.EndVersion)
		}
	}

	var checkVersions []string
	for version := range checks {
		checkVersions = append(checkVersions, version)
	}
	sort.Slice(checkVersions, func(i, j int) bool {
		return versioncmp(checkVersions[i], checkVersions[j]) == -1
	})

	var errs []error
	gherrs := map[string][]string{}
	for _, version := range checkVersions {
		var checkLibs []string
		for lib := range checks[version] {
			checkLibs = append(checkLibs, lib)
		}
		sort.Strings(checkLibs)

		for _, lib := range checkLibs {
			fmt.Printf("[INF] checking %s@%s\n", lib, version)

			pt, err := GetPatcher(version, lib)
			if err != nil {
				fmt.Fprintf(os.Stderr, "[FTL] get patcher: %v\n", err)
				os.Exit(1)
				return
			} else if pt == nil {
				fmt.Printf("[WRN] no data available, skipping\n")
				continue
			}

			_, err = pt.ExtractDynsyms(true)
			if err != nil {
				fmt.Fprintf(os.Stderr, "[FTL] extract symbols: %v\n", err)
				os.Exit(1)
				return
			}

			for _, check := range checks[version][lib] {
				fmt.Printf("[INF] %s:\n        checking for one of %+s\n", check.File, check.Symbols)
				var f bool
				for _, sym := range check.Symbols {
					off, err := pt.ResolveSym(sym)
					if err != nil {
						fmt.Printf("          %s not found\n", sym)
					} else {
						fmt.Printf("          %s found at %#x\n", sym, off)
						f = true
					}
				}
				if !f {
					err := fmt.Errorf("%s: one of %+s not found in %s@%s", check.File, check.Symbols, lib, version)
					fmt.Printf("[ERR] %v\n", err)
					errs = append(errs, err)

					spl := strings.Split(check.File, ":")
					gherrf := fmt.Sprintf("file=%s,line=%s,col=%s", spl[0], spl[1], spl[2])
					gherrs[gherrf] = append(gherrs[gherrf], fmt.Sprintf("one of symbols %+s not found in %s@%s", check.Symbols, lib, version))
				}
			}
		}
	}
	if len(errs) == 0 {
		os.Exit(0)
	}

	fmt.Printf("[FTL] check failed\n")
	for _, err := range errs {
		fmt.Printf("        %v\n", err)
	}
	if os.Getenv("GITHUB_ACTIONS") == "true" {
		var ghfs []string
		for ghf := range gherrs {
			ghfs = append(ghfs, ghf)
		}
		sort.Strings(ghfs)
		for _, ghf := range ghfs {
			fmt.Printf("::error %s::%s\n", ghf, strings.Join(gherrs[ghf], "%0A"))
		}
	}
	os.Exit(1)
}

func GetPatcher(version, lib string) (*patchlib.Patcher, error) {
	resp, err := http.Get("https://github.com/pgaskin/kobopatch-testdata/raw/v1/" + version + ".tar.xz")
	if err != nil {
		return nil, fmt.Errorf("get kobopatch testdata for %#v: %w", version, err)
	}
	defer resp.Body.Close()

	if resp.StatusCode == http.StatusNotFound {
		return nil, nil
	} else if resp.StatusCode != http.StatusOK {
		return nil, fmt.Errorf("get kobopatch testdata for %#v: response status %s", version, resp.Status)
	}

	zr, err := xz.NewReader(resp.Body, 0)
	if err != nil {
		return nil, fmt.Errorf("read kobopatch testdata: %w", err)
	}

	tr := tar.NewReader(zr)
	for {
		th, err := tr.Next()
		if err != nil {
			if err == io.EOF {
				return nil, fmt.Errorf("read kobopatch testdata: file %#v not found", lib)
			}
			return nil, fmt.Errorf("read kobopatch testdata: %w", err)
		}
		if filepath.Clean(th.Name) == filepath.Clean(lib) {
			break
		}
	}

	buf, err := ioutil.ReadAll(tr)
	if err != nil {
		return nil, fmt.Errorf("read kobopatch testdata: %w", err)
	}
	return patchlib.NewPatcher(buf), nil
}

type SymCheck struct {
	File         string
	Library      string
	StartVersion string
	EndVersion   string
	Symbols      []string // or
}

func FindSymChecks(dir string) ([]SymCheck, error) {
	var checks []SymCheck
	if err := filepath.Walk(dir, func(path string, info os.FileInfo, err error) error {
		var m bool
		for _, ext := range []string{".c", ".cc", ".cpp", ".h"} {
			if filepath.Ext(path) == ext {
				m = true
				break
			}
		}
		if !m {
			return nil
		}

		f, err := os.OpenFile(path, os.O_RDONLY, 0)
		if err != nil {
			return fmt.Errorf("open %#v: %w", path, err)
		}
		defer f.Close()

		sc := bufio.NewScanner(f)
		var line int
		for sc.Scan() {
			line++
			col := bytes.Index(sc.Bytes(), []byte("//libnickel"))
			if col == -1 {
				continue
			}

			args := strings.Fields(string(bytes.TrimSpace(sc.Bytes()[col+len("//libnickel"):])))
			if len(args) < 3 || args[0] == "*" {
				return fmt.Errorf("parse %#v: line %d, col %d: expected comment to be in the format '//libnickel <start_version> <end_version|*> <sym>...'", path, line, col+1)
			}

			checks = append(checks, SymCheck{
				File:         fmt.Sprintf("%s:%d:%d", path, line, col+1),
				Library:      "libnickel.so.1.0.0",
				StartVersion: args[0],
				EndVersion:   args[1],
				Symbols:      args[2:],
			})
		}
		if err := sc.Err(); err != nil {
			return fmt.Errorf("read %#v: %w", path, err)
		}

		return nil
	}); err != nil {
		return nil, err
	}
	return checks, nil
}

func versioncmp(a, b string) int {
	if a == "*" || b == "*" {
		return 0
	}
	aspl, bspl := splint(a), splint(b)
	mlen := len(aspl)
	if len(bspl) > mlen {
		mlen = len(bspl)
	}
	for i := 0; i < mlen; i++ {
		switch {
		case i == len(bspl):
			return 1
		case i == len(aspl):
			return -1
		case aspl[i] > bspl[i]:
			return 1
		case bspl[i] > aspl[i]:
			return -1
		}
	}
	return 0
}

func splint(str string) []int64 {
	spl := strings.Split(str, ".")
	ints := make([]int64, len(spl))
	for i, p := range spl {
		ints[i], _ = strconv.ParseInt(p, 10, 64)
	}
	return ints
}
