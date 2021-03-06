#ifndef TAG_H
#define TAG_H

#include <QString>
#include <QStringList>
#include <QSettings>
#include "favorite.h"


class Tag
{
	public:
		Tag();
		explicit Tag(QSettings *settings, QString text, QString type = "unknown", int count = 1, QStringList related = QStringList());
		~Tag();
		QString stylished(QList<Favorite> favs = QList<Favorite>(), QStringList ignored = QStringList(), QStringList blacklisted = QStringList(), bool count = false) const;
		void setText(QString);
		void setType(QString);
		void setCount(int);
		void setRelated(QStringList);
		QString		text()		const;
		QString		type()		const;
		int			shortType()	const;
		int			count()		const;
		QStringList	related()	const;
		QString		typedText()	const;

	private:
		QString		m_text, m_type;
		int			m_count;
		QStringList	m_related;
		QSettings	*m_settings;
};

bool sortByFrequency(Tag, Tag);
bool operator==(const Tag &t1, const Tag &t2);

Q_DECLARE_METATYPE(Tag)

#endif // TAG_H
