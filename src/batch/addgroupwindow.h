#ifndef ADDGROUPWINDOW_H
#define ADDGROUPWINDOW_H

#include <QSpinBox>
#include <QComboBox>
#include "ui/textedit.h"
#include "models/favorite.h"


class AddGroupWindow : public QWidget
{
	Q_OBJECT

	public:
		AddGroupWindow(QString, QStringList, QList<Favorite>, QWidget *parent);

	public slots:
		void ok();

	signals:
		void sendData(QStringList);

	private:
		TextEdit	*m_lineTags;
		QSpinBox	*m_spinPage, *m_spinPP, *m_spinLimit;
		QComboBox	*m_comboDwl, *m_comboSites;
		QStringList m_sites;
};

#endif // ADDGROUPWINDOW_H
