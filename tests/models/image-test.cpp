#include <QtTest>
#include "test-suite.h"
#include "image-test.h"


void ImageTest::init()
{
    m_details["md5"] = "1bc29b36f623ba82aaf6724fd3b16718";
    m_details["ext"] = "jpg";
    m_details["author"] = "superauthor";
    m_details["status"] = "tested";
    m_details["filename"] = "oldfilename";
    m_details["search"] = "testing well";
    m_details["id"] = "7331";
    m_details["score"] = "21";
    m_details["parent_id"] = "1337";
    m_details["file_size"] = "1234567";
    m_details["creator_id"] = "1234";
    m_details["has_children"] = "true";
    m_details["has_note"] = "true";
    m_details["has_comments"] = "true";
    m_details["file_url"] = "http://test.com/img/oldfilename.jpg";
    m_details["sample_url"] = "http://test.com/sample/oldfilename.jpg";
    m_details["preview_url"] = "http://test.com/preview/oldfilename.jpg";
    m_details["width"] = "800";
    m_details["height"] = "600";
    m_details["source"] = "http://google.com/";
    m_details["tags_general"] = "tag1 tag2 tag3";
    m_details["tags_artist"] = "artist1";
    m_details["tags_copyright"] = "copyright1 copyright2";
    m_details["tags_character"] = "character1 character2";
    m_details["created_at"] = "1471513944";
    m_details["rating"] = "safe";

    m_settings = new QSettings("tests/resources/settings.ini", QSettings::IniFormat);
    m_site = new Site(m_settings, "release/sites/Danbooru (2.0)", "danbooru.donmai.us");
    m_img = new Image(m_site, m_details);
}

void ImageTest::cleanup()
{
    m_settings->deleteLater();
    m_site->deleteLater();
    m_img->deleteLater();
}


void ImageTest::testCopy()
{
    Image clone = *m_img;

    QCOMPARE(clone.id(), m_img->id());
    QCOMPARE(clone.md5(), m_img->md5());
    QCOMPARE(clone.tags(), m_img->tags());
    QCOMPARE(clone.author(), m_img->author());
    QCOMPARE(clone.status(), m_img->status());
    QCOMPARE(clone.rating(), m_img->rating());
    QCOMPARE(clone.source(), m_img->source());
    QCOMPARE(clone.site(), m_img->site());
    QCOMPARE(clone.parentSite(), m_img->parentSite());
    QCOMPARE(clone.filename(), m_img->filename());
    QCOMPARE(clone.folder(), m_img->folder());
    QCOMPARE(clone.pools(), m_img->pools());
    QCOMPARE(clone.fileSize(), m_img->fileSize());
    QCOMPARE(clone.score(), m_img->score());
    QCOMPARE(clone.parentId(), m_img->parentId());
    QCOMPARE(clone.width(), m_img->width());
    QCOMPARE(clone.height(), m_img->height());
    QCOMPARE(clone.authorId(), m_img->authorId());
    QCOMPARE(clone.createdAt(), m_img->createdAt());
    QCOMPARE(clone.hasChildren(), m_img->hasChildren());
    QCOMPARE(clone.hasNote(), m_img->hasNote());
    QCOMPARE(clone.hasComments(), m_img->hasComments());
    QCOMPARE(clone.hasScore(), m_img->hasScore());
    QCOMPARE(clone.fileUrl(), m_img->fileUrl());
    QCOMPARE(clone.sampleUrl(), m_img->sampleUrl());
    QCOMPARE(clone.previewUrl(), m_img->previewUrl());
    QCOMPARE(clone.pageUrl(), m_img->pageUrl());
    QCOMPARE(clone.size(), m_img->size());
    QCOMPARE(clone.previewImage(), m_img->previewImage());
    QCOMPARE(clone.page(), m_img->page());
    QCOMPARE(clone.data(), m_img->data());
    QCOMPARE(clone.settings(), m_img->settings());
    QCOMPARE(clone.details(), m_img->details());
    QCOMPARE(clone.search(), m_img->search());
}

void ImageTest::testHasTag()
{
    QCOMPARE(m_img->hasTag("tag1"), true);
    QCOMPARE(m_img->hasTag("character1"), true);
    QCOMPARE(m_img->hasTag("tag2"), true);
    QCOMPARE(m_img->hasTag("tag7"), false);
    QCOMPARE(m_img->hasTag("copyright3"), false);
}
void ImageTest::testHasAnyTag()
{
    QCOMPARE(m_img->hasAnyTag(QStringList() << "tag1" << "tag2"), true);
    QCOMPARE(m_img->hasAnyTag(QStringList() << "tag7" << "tag1"), true);
    QCOMPARE(m_img->hasAnyTag(QStringList() << "tag4" << "tag7"), false);
}
void ImageTest::testHasAllTags()
{
    QCOMPARE(m_img->hasAllTags(QStringList() << "tag1" << "tag2"), true);
    QCOMPARE(m_img->hasAllTags(QStringList() << "tag7" << "tag1"), false);
    QCOMPARE(m_img->hasAllTags(QStringList() << "tag4" << "tag7"), false);
}

void ImageTest::testUnload()
{
    m_img->setData(QString("test").toLatin1());
    QCOMPARE(m_img->data().isEmpty(), false);

    m_img->unload();
    QCOMPARE(m_img->data().isEmpty(), true);
}

void ImageTest::testBlacklisted()
{
    // Basic
    QCOMPARE(m_img->blacklisted(QStringList() << "tag8" << "tag7"), QStringList());
    QCOMPARE(m_img->blacklisted(QStringList() << "tag1" << "tag7"), QStringList() << "tag1");
    QCOMPARE(m_img->blacklisted(QStringList() << "character1" << "artist1"), QStringList() << "character1" << "artist1");

    // Invert
    QCOMPARE(m_img->blacklisted(QStringList() << "tag8" << "tag7", false), QStringList() << "tag8" << "tag7");
    QCOMPARE(m_img->blacklisted(QStringList() << "tag1" << "tag7", false), QStringList() << "tag7");
    QCOMPARE(m_img->blacklisted(QStringList() << "character1" << "artist1", false), QStringList());
}

void ImageTest::testMatchTag()
{
    // Basic
    QCOMPARE(m_img->match("tag1"), QString());
    QCOMPARE(m_img->match("character1"), QString());
    QCOMPARE(m_img->match("tag7"), QString("image does not contains \"tag7\""));

    // Minus
    QCOMPARE(m_img->match("-tag1"), QString("image contains \"tag1\""));
    QCOMPARE(m_img->match("-character1"), QString("image contains \"character1\""));
    QCOMPARE(m_img->match("-tag7"), QString());

    // Invert
    QCOMPARE(m_img->match("tag1", true), QString("image contains \"tag1\""));
    QCOMPARE(m_img->match("character1", true), QString("image contains \"character1\""));
    QCOMPARE(m_img->match("tag7", true), QString());

    // Invert minus
    QCOMPARE(m_img->match("-tag1", true), QString());
    QCOMPARE(m_img->match("-character1", true), QString());
    QCOMPARE(m_img->match("-tag7", true), QString("image does not contains \"tag7\""));
}

void ImageTest::testMatchRating()
{
    // Basic
    QCOMPARE(m_img->match("rating:safe"), QString());
    QCOMPARE(m_img->match("rating:explicit"), QString("image is not \"explicit\""));

    // Short versions
    QCOMPARE(m_img->match("rating:s"), QString());
    QCOMPARE(m_img->match("rating:e"), QString("image is not \"explicit\""));

    // Invert
    QCOMPARE(m_img->match("rating:safe", true), QString("image is \"safe\""));
    QCOMPARE(m_img->match("rating:explicit", true), QString());
}

void ImageTest::testValue()
{
    // Guess from image size
    QCOMPARE(m_img->value(), 800 * 600);

    // Even with a tag, still use image size if possible
    m_details["tags_general"] = "lowres";
    m_img->deleteLater();
    m_img = new Image(m_site, m_details);
    QCOMPARE(m_img->value(), 800 * 600);

    // Default value if nothing is given
    m_details.remove("width");
    m_details.remove("height");
    m_details["tags_general"] = "";
    m_img->deleteLater();
    m_img = new Image(m_site, m_details);
    QCOMPARE(m_img->value(), 1200 * 900);

    m_details["tags_general"] = "incredibly_absurdres";
    m_img->deleteLater();
    m_img = new Image(m_site, m_details);
    QCOMPARE(m_img->value(), 10000 * 10000);

    m_details["tags_general"] = "absurdres";
    m_img->deleteLater();
    m_img = new Image(m_site, m_details);
    QCOMPARE(m_img->value(), 3200 * 2400);

    m_details["tags_general"] = "highres";
    m_img->deleteLater();
    m_img = new Image(m_site, m_details);
    QCOMPARE(m_img->value(), 1600 * 1200);

    m_details["tags_general"] = "lowres";
    m_img->deleteLater();
    m_img = new Image(m_site, m_details);
    QCOMPARE(m_img->value(), 500 * 500);
}


static ImageTest instance;