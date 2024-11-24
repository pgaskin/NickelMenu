#ifndef NM_DIALOG_H
#define NM_DIALOG_H

#include <QDialog>

class NMDialog : public QDialog
{
    Q_OBJECT
    public:
        NMDialog(QWidget* parent = nullptr);
        ~NMDialog();
        void showDlg();

    protected:
        //bool event(QEvent *event) override;
        bool eventFilter(QObject *obj, QEvent *event) override;
    private:
        void installEvFilter(QWidget *w);
};

#endif // NM_DIALOG_H