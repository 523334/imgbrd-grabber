#include <QFile>
#include <QNetworkCookie>
#include <QNetworkCookieJar>
#include <QNetworkDiskCache>
#include <QUrlQuery>
#include <QDir>
#include <QTimer>
#include "site.h"
#include "functions.h"
#include "json.h"
#include "page.h"
#include "image.h"
#include "qcustomnetworkreply.h"
#ifdef QT_DEBUG
	#include <QDebug>
	#define CACHE_POLICY QNetworkRequest::PreferCache
#else
	#define CACHE_POLICY QNetworkRequest::PreferNetwork
#endif



QMap<QString, Site*> *g_allSites = Q_NULLPTR;

static void _prependUrl(QMap<QString,QString> *details, QString url, QString key, QString lkey = QString())
{
	if (details->contains(key))
		details->insert(lkey == NULL ? key : lkey, url + details->value(key));
}

Site::Site(QSettings *settings, QString dir, QString url)
	: m_url(url), m_settings(nullptr), m_manager(nullptr), m_cookieJar(nullptr), m_loggedIn(false), m_triedLogin(false), m_loginCheck(false), m_updateVersion("")
{
	QFile file(dir + "/model.xml");
	if (file.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		QFileInfo info(dir);
		m_type = info.fileName();

		QString source = file.readAll();
		QDomDocument doc;
		QString errorMsg;
		int errorLine, errorColumn;
		if (!doc.setContent(source, false, &errorMsg, &errorLine, &errorColumn))
		{ log(tr("Erreur lors de l'analyse du fichier XML : %1 (%2 - %3).").arg(errorMsg, QString::number(errorLine), QString::number(errorColumn)), Error); }
		else
		{
			QDomElement docElem = doc.documentElement();
			QMap<QString,QString> detals = domToMap(docElem);
			QStringList defaults = QStringList() << "xml" << "json" << "rss" << "regex";
			QStringList source;
			for (int s = 0; s < 4; s++)
			{
				QString t = settings->value("source_"+QString::number(s+1), defaults.at(s)).toString();
				t[0] = t[0].toUpper();
				if (detals.contains("Urls/"+(t == "Regex" ? "Html" : t)+"/Tags"))
				{ source.append(t); }
			}
			if (!source.isEmpty())
			{
				QStringList srcs;
				QSettings sets(dir + "/" + url + "/settings.ini", QSettings::IniFormat);
				if (!sets.value("sources/usedefault", true).toBool())
				{
					srcs = QStringList() << sets.value("sources/source_1").toString()
										 << sets.value("sources/source_2").toString()
										 << sets.value("sources/source_3").toString()
										 << sets.value("sources/source_4").toString();
					srcs.removeAll("");
					if (srcs.isEmpty())
					{ srcs = source; }
					else
					{
						for (int i = 0; i < srcs.size(); i++)
						{ srcs[i][0] = srcs[i][0].toUpper(); }
					}
				}
				else
				{ srcs = source; }

				QMap<QString,QString> details = detals;
				details["Model"] = m_type;
				details["Url"] = url;
				details["Selected"] = srcs.join("/").toLower();
				QString lineSsl = QString(sets.value("ssl", false).toBool() ? "https" : "http") + "://" + url;
				for (int j = 0; j < srcs.size(); j++)
				{
					QString sr = srcs[j] == "Regex" ? "Html" : srcs[j];
					_prependUrl(&details, lineSsl, "Urls/"+sr+"/Tags", "Urls/"+QString::number(j+1)+"/Tags");
					_prependUrl(&details, lineSsl, "Urls/"+sr+"/Home", "Urls/"+QString::number(j+1)+"/Home");
					_prependUrl(&details, lineSsl, "Urls/"+sr+"/Pools", "Urls/"+QString::number(j+1)+"/Pools");
					if (details.contains("Urls/"+sr+"/Login"))
						details["Urls/"+QString::number(j+1)+"/Login"] = details["Urls/"+sr+"/Login"];
					if (details.contains("Urls/"+sr+"/Limit"))
						details["Urls/"+QString::number(j+1)+"/Limit"] = details["Urls/"+sr+"/Limit"];
					if (details.contains("Urls/"+sr+"/Image"))
						details["Urls/"+QString::number(j+1)+"/Image"] = details["Urls/"+sr+"/Image"];
					if (details.contains("Urls/"+sr+"/Sample"))
						details["Urls/"+QString::number(j+1)+"/Sample"] = details["Urls/"+sr+"/Sample"];
					if (details.contains("Urls/"+sr+"/Preview"))
						details["Urls/"+QString::number(j+1)+"/Preview"] = details["Urls/"+sr+"/Preview"];
					if (details.contains("Urls/"+sr+"/MaxPage"))
						details["Urls/"+QString::number(j+1)+"/MaxPage"] = details["Urls/"+sr+"/MaxPage"];
					if (details.contains("Urls/"+sr+"/AltPagePrev"))
						details["Urls/"+QString::number(j+1)+"/AltPagePrev"] = details["Urls/"+sr+"/AltPagePrev"];
					if (details.contains("Urls/"+sr+"/AltPageNext"))
						details["Urls/"+QString::number(j+1)+"/AltPageNext"] = details["Urls/"+sr+"/AltPageNext"];
				}
				_prependUrl(&details, lineSsl, "Urls/Html/Post");
				_prependUrl(&details, lineSsl, "Urls/Html/Tags");
				_prependUrl(&details, lineSsl, "Urls/Html/Home");
				_prependUrl(&details, lineSsl, "Urls/Html/Pools");

				m_data = details;
			}
			else
			{ log(tr("Aucune source valide trouvée dans le fichier model.xml de %1.").arg(m_type)); }
		}
		file.close();
	}

	load(dir + "/" + url + "/settings.ini");
}
Site::Site(QString type, QString url, QMap<QString, QString> data)
	: m_type(type), m_url(url), m_data(data), m_settings(nullptr), m_manager(nullptr), m_cookieJar(nullptr), m_loggedIn(false), m_triedLogin(false), m_loginCheck(false), m_updateVersion("")
{
	load();
}

Site::~Site()
{
	m_settings->deleteLater();

	//m_manager->deleteLater();
	//m_cookieJar->deleteLater();
}

/**
 * Load or reload the settings.
 */
void Site::load(QString path)
{
	// Delete settings if necessary
	if (m_settings != nullptr)
	{ m_settings->deleteLater(); }
	m_settings = new QSettings(path.isEmpty() ? savePath("sites/"+m_type+"/"+m_url+"/settings.ini") : path, QSettings::IniFormat);
	m_name = m_settings->value("name", m_url).toString();

	// Auth information
	m_username = m_settings->value("auth/pseudo", "").toString();
	m_password = m_settings->value("auth/password", "").toString();

	// Cookies
	m_cookies.clear();
	QList<QVariant> cookies = m_settings->value("cookies", "").toList();
	for (QVariant variant : cookies)
	{
		QList<QNetworkCookie> cookiz = QNetworkCookie::parseCookies(variant.toByteArray());
		for (QNetworkCookie cookie : cookiz)
		{
			cookie.setDomain(m_url);
			cookie.setPath("/");
			m_cookies.append(cookie);
		}
	}
	if (m_cookieJar != nullptr)
	{ resetCookieJar(); }
}

/**
 * Initialize or reset the site's cookie jar.
 */
void Site::resetCookieJar()
{
	// Delete cookie jar if necessary
	if (m_cookieJar != nullptr)
	{ m_cookieJar->deleteLater(); }

	m_cookieJar = new QNetworkCookieJar(m_manager);

	for (QNetworkCookie cookie : m_cookies)
	{ m_cookieJar->insertCookie(cookie); }

	m_manager->setCookieJar(m_cookieJar);
	m_loggedIn = false;
	m_triedLogin = false;
}

/**
 * Initialize the network manager.
 */
void Site::initManager()
{
	if (m_manager == nullptr)
	{
		// Create the access manager and get its slots
		m_manager = new QNetworkAccessManager(this);
		connect(m_manager, SIGNAL(finished(QNetworkReply*)), this, SIGNAL(finished(QNetworkReply*)));
		connect(m_manager, SIGNAL(sslErrors(QNetworkReply*,QList<QSslError>)), this, SLOT(sslErrorHandler(QNetworkReply*,QList<QSslError>)));

		// Cache
		QNetworkDiskCache *diskCache = new QNetworkDiskCache(m_manager);
		diskCache->setCacheDirectory(savePath("cache/"));
		m_manager->setCache(diskCache);

		// Cookies
		resetCookieJar();
	}
}

/**
 * Try to log into the website.
 *
 * @param force	Whether to force login or not
 */
void Site::login(bool force)
{
	if (!m_settings->value("login/parameter").toBool() && (force || (!m_loggedIn && !m_triedLogin)))
	{
		if (!m_settings->value("login/url", "").toString().isEmpty())
		{
			log(tr("Connexion à %1 (%2)...").arg(m_name, m_url));
			initManager();

			if (force)
			{ resetCookieJar(); }

			m_triedLogin = true;

			QUrlQuery query;
			query.addQueryItem(m_settings->value("login/pseudo", "").toString(), m_username);
			query.addQueryItem(m_settings->value("login/password", "").toString(), m_password);

			QString method = m_settings->value("login/method", "post").toString();
			if (method == "post")
			{
				QUrl postData;
				postData.setQuery(query);

				QNetworkRequest request(fixUrl(m_settings->value("login/url", "").toString()));
				request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

				m_loginReply = m_manager->post(request, query.query(QUrl::FullyEncoded).toUtf8());
				connect(m_loginReply, SIGNAL(finished()), this, SLOT(loginFinished()));
			}
			else
			{
				QUrl url = fixUrl(m_settings->value("login/url", "").toString());
				qDebug() << url;
				url.setQuery(query);
				qDebug() << url;

				QNetworkRequest request(url);
				request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, CACHE_POLICY);

				m_loginReply = getRequest(request);
				connect(m_loginReply, SIGNAL(finished()), this, SLOT(loginFinished()));
			}

			return;
		}
	}

	emit loggedIn(this, LoginNoLogin);
}

/**
 * Called when the login try is finished.
 */
void Site::loginFinished()
{
	m_loggedIn = false;
	QString cookiename = m_settings->value("login/cookie", "").toString();

	QList<QNetworkCookie> cookies = m_cookieJar->cookiesForUrl(m_loginReply->url());
	for (QNetworkCookie cookie : cookies)
	{
		if (cookie.name() == cookiename && !cookie.value().isEmpty())
		{ m_loggedIn = true; }
	}

	log(tr("Connexion à %1 (%2) terminée (%3).").arg(m_name, m_url, m_loggedIn ? tr("succès") : tr("échec")));

	emit loggedIn(this, m_loggedIn ? LoginSuccess : LoginError);
}

QNetworkRequest Site::makeRequest(QUrl url, Page *page, QString ref, Image *img)
{
	if (!m_loggedIn && !m_triedLogin)
		login();

	// Force HTTPS if set so in the settings (no mixed content allowed)
	if (m_settings->value("ssl", false).toBool() && url.scheme() == "http")
		url.setScheme("https");

	QNetworkRequest request(url);
	QString referer = m_settings->value("referer"+(!ref.isEmpty() ? "_"+ref : ""), "").toString();
	if (referer.isEmpty() && !ref.isEmpty())
	{ referer = m_settings->value("referer", "none").toString(); }
	if (referer != "none" && (referer != "page" || page != NULL))
	{
		QString ref;
		if (referer == "host")
		{ ref = url.scheme()+"://"+url.host(); }
		else if (referer == "image")
		{ ref = url.toString(); }
		else if (referer == "page" && page)
		{ ref = page->url().toString(); }
		else if (referer == "details" && img)
		{ ref = img->pageUrl().toString(); }
		request.setRawHeader("Referer", ref.toLatin1());
	}

	QMap<QString,QVariant> headers = m_settings->value("headers").toMap();
	request.setRawHeader("User-Agent", "Mozilla/5.0 (Windows NT 6.1; WOW64; rv:35.0) Gecko/20100101 Firefox/35.0");
	for (QString key : headers.keys())
	{ request.setRawHeader(key.toLatin1(), headers[key].toString().toLatin1()); }

	initManager();
	request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, CACHE_POLICY);
	return request;
}

/**
 * Get an URL from the site.
 *
 * @param url	The URL to get
 * @param page	The related page
 * @param ref	The type of referer to use (page, image, etc.)
 * @param img	The related image
 * @return		The equivalent network request
 */
void Site::getAsync(QueryType type, QUrl url, std::function<void(QNetworkReply*)> callback, Page *page, QString ref, Image *img)
{
	m_lastCallback = callback;
	m_callbackRequest = this->makeRequest(url, page, ref, img);

	int sinceLastRequest = m_lastRequest.msecsTo(QDateTime::currentDateTime());

	QString key = (type == QueryType::Retry ? "retry" : (type == QueryType::List ? "page" : (type == QueryType::Img ? "image" : (type == QueryType::Thumb ? "thumbnail" : "details"))));
	int ms = setting("download/throttle_" + key, 0).toInt() * 1000;
	ms -= sinceLastRequest;

	if (ms > 0)
	{ QTimer::singleShot(ms, this, SLOT(getCallback())); }
	else
	{ getCallback(); }
}

void Site::getCallback()
{
	m_lastCallback(this->getRequest(m_callbackRequest));
}

QNetworkReply *Site::get(QUrl url, Page *page, QString ref, Image *img)
{
	QNetworkRequest request = this->makeRequest(url, page, ref, img);
	return this->getRequest(request);
}

QNetworkReply *Site::getRequest(QNetworkRequest request)
{
	m_lastRequest = QDateTime::currentDateTime();

	#ifdef TEST
		QString md5 = QString(QCryptographicHash::hash(request.url().toString().toLatin1(), QCryptographicHash::Md5).toHex());
		QString filename = request.url().fileName();
		QString ext = filename.contains('.') ? filename.mid(filename.lastIndexOf('.') + 1) : "html";
		QString path = "tests/resources/" + m_url + "/" + md5 + "." + ext;

		QFile f(path);
		if (!f.open(QFile::ReadOnly))
		{
			md5 = QString(QCryptographicHash::hash(request.url().toString(QUrl::RemoveQuery).toLatin1(), QCryptographicHash::Md5).toHex());
			f.setFileName("tests/resources/" + m_url + "/" + md5 + "." + ext);

			if (!f.open(QFile::ReadOnly))
			{
				// LCOV_EXCL_START
				if (ext != "jpg" && ext != "png")
				{
					qDebug() << ("Test file not found: " + f.fileName() + " (" + request.url().toString() + ")");
					return nullptr;
				}
				// LCOV_EXCL_STOP

				f.setFileName("tests/resources/image_1x1.png");

				// LCOV_EXCL_START
				if (!f.open(QFile::ReadOnly))
					return nullptr;
				// LCOV_EXCL_STOP
			}
		}

		qDebug() << ("Reply from file: " + request.url().toString() + " -> " + f.fileName());
		QByteArray content = f.readAll();

		QCustomNetworkReply *reply = new QCustomNetworkReply();
		reply->setHttpStatusCode(200, "OK");
		reply->setContentType("text/html");
		reply->setContent(content);

		return reply;
	#else
		return m_manager->get(request);
	#endif
}

/**
 * Log SSL errors in debug mode only.
 *
 * @param qnr		The network reply who generated the SSL errors
 * @param errors	The list of SSL errors that occured
 */
void Site::sslErrorHandler(QNetworkReply* qnr, QList<QSslError> errors)
{
	#ifdef QT_DEBUG
		qDebug() << errors;
	#else
		Q_UNUSED(errors);
	#endif
	qnr->ignoreSslErrors();
}

/**
 * Called when a reply is finished.
 *
 * @param r	The finished reply
 */
void Site::finishedReply(QNetworkReply *r)
{
	if (r != m_loginReply)
		emit finished(r);
}

/**
 * Check if an update is available for this source's model file.
 */
void Site::checkForUpdates()
{
	QString path = m_settings->value("models", "https://raw.githubusercontent.com/Bionus/imgbrd-grabber/master/release/sites/").toString();
	QString url = path + m_type + "/model.xml";

	initManager();

	m_updateReply = get(QUrl(url));
	connect(m_updateReply, SIGNAL(finished()), this, SLOT(checkForUpdatesDone()));
}

/**
 * Called when the update check is finished.
 */
void Site::checkForUpdatesDone()
{
	QString source = m_updateReply->readAll();
	if (source.startsWith("<?xml"))
	{
		QFile current(savePath("sites/"+m_type+"/model.xml"));
		if (current.open(QFile::ReadOnly))
		{
			QString compare = current.readAll();
			current.close();

			if (compare != source)
			{ m_updateVersion = VERSION; }
		}
	}
	emit checkForUpdatesFinished(this);
}

QList<Site*> Site::getSites(QStringList sources)
{
	QMap<QString, Site*> *sites = Site::getAllSites();

	QList<Site*> ret;
	for (QString source : sources)
	{
		if (sites->contains(source))
		{ ret.append(sites->value(source)); }
	}

	return ret;
}
QMap<QString, Site*> *Site::getAllSites()
{
	if (g_allSites != Q_NULLPTR)
		return g_allSites;

	QMap<QString, Site*> *stes = new QMap<QString, Site*>();
	QSettings settings(savePath("settings.ini"), QSettings::IniFormat);
	QStringList dirs = QDir(savePath("sites")).entryList(QDir::Dirs | QDir::NoDotAndDotDot);

	for (QString dir : dirs)
	{
		QFile f(savePath("sites/"+dir+"/sites.txt"));
		if (f.open(QIODevice::ReadOnly | QIODevice::Text))
		{
			while (!f.atEnd())
			{
				QString line = f.readLine().trimmed();
				if (line.isEmpty())
					continue;

				stes->insert(line, new Site(&settings, savePath("sites/"+dir), line));
			}
		}
	}

	g_allSites = stes;
	return g_allSites;
}

void Site::loadTags(int page, int limit)
{
	initManager();

	QString protocol = (m_settings->value("ssl", false).toBool() ? "https" : "http");
	m_tagsReply = get(QUrl(protocol + "://"+m_url+"/tags.json?search[hide_empty]=yes&limit="+QString::number(limit)+"&page=" + QString::number(page)));
	connect(m_tagsReply, SIGNAL(finished()), this, SLOT(finishedTags()));
}

void Site::finishedTags()
{
	QString source = m_tagsReply->readAll();
	m_tagsReply->deleteLater();
	QList<Tag> tags;
	QVariant src = Json::parse(source);
	if (!src.isNull())
	{
		QList<QVariant> sourc = src.toList();
		for (int id = 0; id < sourc.count(); id++)
		{
			QMap<QString, QVariant> sc = sourc.at(id).toMap();
			int cat = sc.value("category").toInt();
			tags.append(Tag(m_settings,
							sc.value("name").toString(),
							cat == 0 ? "general" : (cat == 1 ? "artist" : (cat == 3 ? "copyright" : "character")),
							sc.value("post_count").toInt(),
							sc.value("related_tags").toString().split(' ')));
		}
	}
	emit finishedLoadingTags(tags);
}

bool Site::contains(QString what)	{ return m_data.contains(what);	}
QString Site::value(QString what)	{ return m_data.value(what);	}

QVariant Site::setting(QString key, QVariant def)	{ return m_settings->value(key, def); }
QSettings	*Site::settings()						{ return m_settings; }

QString Site::name()			{ return m_name;			}
QString Site::url()				{ return m_url;				}
QString Site::type()			{ return m_type;			}
QString Site::updateVersion()	{ return m_updateVersion;	}
QString Site::username()		{ return m_username;		}
QString Site::password()		{ return m_password;		}

void Site::setUsername(QString username)	{ m_username = username;	}
void Site::setPassword(QString password)	{ m_password = password;	}

QUrl Site::fixUrl(QString url)
{
	return this->fixUrl(url, QUrl());
}

QUrl Site::fixUrl(QString url, QUrl old)
{
	if (url.isEmpty())
		return QUrl();

	QString protocol = (m_settings->value("ssl", false).toBool() ? "https" : "http");

	if (url.startsWith("//"))
	{ return QUrl(protocol + ":" + url); }
	if (url.startsWith("/"))
	{ return QUrl(protocol + "://" + m_data.value("Url") + url); }

	if (!url.startsWith("http"))
	{
		if (old.isValid())
		{ return old.resolved(QUrl(url)); }
		return QUrl(protocol + "://" + m_data.value("Url") + "/" + url);
	}

	return QUrl(url);
}

QList<QNetworkCookie> Site::cookies()
{
	return m_cookies;
}
