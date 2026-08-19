/*
 *  This file is part of skyscraper.
 *  Copyright 2024 Gemba @ GitHub
 *
 *  skyscraper is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  skyscraper is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with skyscraper; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.
 */

#include "pixl.h"

#include "pathtools.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QStringBuilder>
#include <QDebug>
#include <QDir>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QStringBuilder>
#include <QStringList>
#include <QTemporaryFile>
#include <QTextStream>


Pixl::Pixl() {}

static const QString baseFolder() { return QString("/recalbox/share/roms/"); }

QStringList Pixl::extraGamelistTags(bool isFolder) {
    (void)isFolder;
    // does not require extra XML elements for the moment
    return QStringList();
}

QStringList Pixl::createEsVariantXml(const GameEntry &entry) {
    QStringList l;
    bool addEmptyElem = addEmptyElement() && !entry.isFolder;

    l.append(elem(GameEntry::getTag(GameEntry::Elem::COVER),
                  entry.coverFile, addEmptyElem, true));
    l.append(elem(GameEntry::getTag(GameEntry::Elem::THREEDCOVER),
                  entry.threedcoverFile, addEmptyElem, true));
    l.append(elem(GameEntry::getTag(GameEntry::Elem::FULLCOVER),
                  entry.fullcoverFile, addEmptyElem, true));
    l.append(elem(GameEntry::getTag(GameEntry::Elem::SCREENSHOT),
                  entry.screenshotFile, addEmptyElem, true));
    l.append(elem(GameEntry::getTag(GameEntry::Elem::SCREENSHOTTITLE),
                  entry.screenshottitleFile, addEmptyElem, true));
    l.append(elem(GameEntry::getTag(GameEntry::Elem::MARQUEE),
                  entry.marqueeFile, addEmptyElem, true));
    l.append(elem(GameEntry::getTag(GameEntry::Elem::WHEEL),
                  entry.wheelFile, addEmptyElem, true));
    l.append(elem(GameEntry::getTag(GameEntry::Elem::TEXTURE),
                  entry.textureFile, addEmptyElem, true));
    l.append(elem(GameEntry::getTag(GameEntry::Elem::VIDEO),
                  entry.videoFile,addEmptyElem, true));
    l.append(elem(GameEntry::getTag(GameEntry::Elem::MANUAL),
                  entry.manualFile, addEmptyElem, true));
    l.append(elem(GameEntry::getTag(GameEntry::Elem::MAP),
                  entry.mapFile, addEmptyElem, true));
    l.append(elem(GameEntry::getTag(GameEntry::Elem::FANART),
                  entry.fanartFile, addEmptyElem, true));
    l.append(elem(GameEntry::getTag(GameEntry::Elem::BACKCOVER),
                  entry.backcoverFile, addEmptyElem, true));

    return l;
}

QString Pixl::getTargetFilePath(const GameEntry::Types t,
                                            const QString &baseName,
                                            const QString &subPath,
                                            const QString &cacheFn,
                                            QString ext) {
    QString fnExt = ext;
    if (ext.isEmpty()) {
        QMimeType mime = mimeDb.mimeTypeForFile(cacheFn);
        fnExt = mime.preferredSuffix().replace("jpeg", "jpg");
        if (fnExt.isEmpty())
            qDebug() << "No mime type detected for" << baseName;
    }
    QString fp;
    switch (t) {
    case GameEntry::BACKCOVER:
        fp = getBackcoversFolder();
        break;
    case GameEntry::COVER:
        fp = getCoversFolder();
        break;
    case GameEntry::FANART:
        fp = getFanartsFolder();
        break;
    case GameEntry::FULLCOVER:
        fp = getFullcoversFolder();
        break;
    case GameEntry::MANUAL:
        fp = getManualsFolder();
        break;
    case GameEntry::MAP:
        fp = getMapsFolder();
        break;
    case GameEntry::MARQUEE:
        fp = getMarqueesFolder();
        break;
    case GameEntry::SCREENSHOT:
        fp = getScreenshotsFolder();
        break;
    case GameEntry::SCREENSHOTTITLE:
        fp = getScreenshottitlesFolder();
        break;
    case GameEntry::TEXTURE:
        fp = getTexturesFolder();
        break;
    case GameEntry::THREEDCOVER:
        fp = get3dcoversFolder();
        break;
    case GameEntry::VIDEO:
        fp = getVideosFolder();
        break;
    case GameEntry::WHEEL:
        fp = getWheelsFolder();
        break;
    default:
        break;
    }
    QString fn = getTargetFileName(t, baseName);
    if (!fp.isEmpty() && !fn.isEmpty()) {
        if (fnExt.isEmpty()) {
            if (QSysInfo::prettyProductName().startsWith("Batocera")) {
                fnExt = defaultMimeType(fn);
            } else {
                // flag other edge cases
                qWarning()
                    << QString(
                           "MIME type not detected for '%1'. Is mime type db "
                           "(/usr/share/mime/packages/freedesktop.org.xml) "
                           "missing on this system?")
                           .arg(fn);
            }
        }
        fp = fp % QString("/%1/%2.%3").arg(subPath).arg(fn).arg(fnExt);
        QFileInfo fi = QFileInfo(fp);
        QDir d = QDir(fi.absolutePath());
        if (!d.exists()) {
            if (!d.mkpath(".")) {
                qWarning() << "Path could not be created" << fi.absolutePath()
                           << " Check file permissions, gamelist binary data "
                              "maybe incomplete.";
            }
        }
        fp = PathTools::lexicallyNormalPath(fp);
    } else {
        fp = "";
    }
    return fp;
}

bool Pixl::copyMedia(GameEntry::Types &savedMedia,
                     const QString &baseName,
                     const QString &subPath, GameEntry &game) {
    bool copyError = false;
    bool success = false;
    GameEntry::Types toCopy =
        supportedMedia() & (savedMedia ^ GameEntry::MEDIA);

    if (!config->backcovers && !config->cacheBackcovers)
        toCopy ^= GameEntry::BACKCOVER;
    if (!config->cacheCovers)
        toCopy ^= GameEntry::COVER;
    if (!config->fanart && !config->cacheFanarts)
        toCopy ^= GameEntry::FANART;
    if (!config->cacheFullcovers)
        toCopy ^= GameEntry::FULLCOVER;
    if (!config->manuals && !config->cacheManuals)
        toCopy ^= GameEntry::MANUAL;
    if (!config->cacheMaps)
        toCopy ^= GameEntry::MAP;
    if (!config->cacheMarquees)
        toCopy ^= GameEntry::MARQUEE;
    if (!config->cacheScreenshots)
        toCopy ^= GameEntry::SCREENSHOT;
    if (!config->cacheScreenshottitles)
        toCopy ^= GameEntry::SCREENSHOTTITLE;
    if (!config->cacheTextures)
        toCopy ^= GameEntry::TEXTURE;
    if (!config->cache3dcovers)
        toCopy ^= GameEntry::THREEDCOVER;
    if (!config->videos && !config->cacheVideos)
        toCopy ^= GameEntry::VIDEO;
    if (!config->cacheWheels)
        toCopy ^= GameEntry::WHEEL;

    qDebug() << "toCopy" << toCopy;
    QList<MediaProps> medias;

    if (GameEntry::BACKCOVER & toCopy) {
        MediaProps m =
            MediaProps(GameEntry::BACKCOVER, game.backcoverData,
                       game.backcoverFile, config->skipExistingBackcovers);
        medias.append(m);
    }

    if (GameEntry::COVER & toCopy) {
        MediaProps m = MediaProps(GameEntry::COVER, game.coverData,
                                  game.coverFile, config->skipExistingCovers);
        medias.append(m);
    }
    if (GameEntry::FANART & toCopy) {
        MediaProps m = MediaProps(GameEntry::FANART, game.fanartData,
                                  game.fanartFile, config->skipExistingFanart);
        medias.append(m);
    }
    if (GameEntry::FULLCOVER & toCopy) {
        MediaProps m = MediaProps(GameEntry::FULLCOVER, game.fullcoverData,
                                  game.fullcoverFile, config->skipExistingFullcovers);
        medias.append(m);
    }
    if (GameEntry::MANUAL & toCopy) {
        MediaProps m = MediaProps(GameEntry::MANUAL, game.manualData,
                                  game.manualFile, config->skipExistingManuals);
        medias.append(m);
    }
    if (GameEntry::MAP & toCopy) {
        MediaProps m = MediaProps(GameEntry::MAP, game.mapData,
                                  game.mapFile, config->skipExistingMaps);
        medias.append(m);
    }
    if (GameEntry::MARQUEE & toCopy) {
        MediaProps m = MediaProps(GameEntry::MARQUEE, game.marqueeData,
                                  game.marqueeFile, config->skipExistingMarquees);
        medias.append(m);
    }
    if (GameEntry::SCREENSHOT & toCopy) {
        MediaProps m =
            MediaProps(GameEntry::SCREENSHOT, game.screenshotData,
                       game.screenshotFile, config->skipExistingScreenshots);
        medias.append(m);
    }
    if (GameEntry::SCREENSHOTTITLE & toCopy) {
        MediaProps m =
            MediaProps(GameEntry::SCREENSHOTTITLE, game.screenshottitleData,
                       game.screenshottitleFile, config->skipExistingScreenshottitles);
        medias.append(m);
    }
    if (GameEntry::THREEDCOVER & toCopy) {
        MediaProps m =
            MediaProps(GameEntry::THREEDCOVER, game.threedcoverData,
                       game.threedcoverFile, config->skipExisting3dcovers);
        medias.append(m);
    }
    if (GameEntry::TEXTURE & toCopy) {
        MediaProps m =
            MediaProps(GameEntry::TEXTURE, game.textureData, game.textureFile,
                       config->skipExistingTextures);
        medias.append(m);
    }
    if (GameEntry::VIDEO & toCopy) {
        MediaProps m = MediaProps(GameEntry::VIDEO, game.videoData,
                                  game.videoFile, config->skipExistingVideos);
        m.ext = game.videoFormat;
        medias.append(m);
    }
    if (GameEntry::WHEEL & toCopy) {
        MediaProps m = MediaProps(GameEntry::WHEEL, game.wheelData,
                                  game.wheelFile, config->skipExistingWheels);
        m.ext = game.videoFormat;
        medias.append(m);
    }

    for (auto &mm : medias) {
        bool putInGamelist = gamelistHasMediaPaths();
        QString tgt;
        // qDebug() << mm.type;
        // qDebug() << *mm.file;
        // qDebug() << mm.data->size();
        if (!mm.file->isEmpty() && !mm.data->isEmpty()) {
            tgt = getTargetFilePath(mm.type, baseName, subPath, *mm.file);
            qDebug() << "tgt" << tgt;
            if (!tgt.isEmpty()) {
                success = doCopy(mm.type, *mm.file, tgt, *mm.data, mm.skip);
                copyError |= !success;
                putInGamelist &= success;
            }
        }
        mm.file->clear();
        if (!putInGamelist || tgt.isEmpty()) {
            mm.data->clear();
        } else {
            mm.file->append(tgt);
        }
    }

    GameEntry::Types drop = (toCopy | savedMedia) ^ GameEntry::MEDIA;
    qDebug() << "drop" << drop;
    if (drop & GameEntry::BACKCOVER)
        game.backcoverFile.clear();
    if (drop & GameEntry::COVER)
        game.coverFile.clear();
    if (drop & GameEntry::FANART)
        game.fanartFile.clear();
    if (drop & GameEntry::FULLCOVER)
        game.fullcoverFile.clear();
    if (drop & GameEntry::MANUAL)
        game.manualFile.clear();
    if (drop & GameEntry::MAP)
        game.mapFile.clear();
    if (drop & GameEntry::MARQUEE)
        game.marqueeFile.clear();
    if (drop & GameEntry::SCREENSHOT)
        game.screenshotFile.clear();
    if (drop & GameEntry::SCREENSHOTTITLE)
        game.screenshottitleFile.clear();
    if (drop & GameEntry::TEXTURE)
        game.textureFile.clear();
    if (drop & GameEntry::THREEDCOVER)
        game.threedcoverFile.clear();
    if (drop & GameEntry::VIDEO)
        game.videoFile.clear();
    if (drop & GameEntry::WHEEL)
        game.wheelFile.clear();

    return copyError;
}

bool Pixl::doCopy(GameEntry::Types t, const QString &cacheFn,
                              QString &tgt, const QByteArray &data,
                              bool skipExisting) {
    bool success = skipExisting;
    QString altTgt = tgt;
    if (t & GameEntry::IMAGE) {
        // depending on source it may have PNG or JPG
        // format: prepare remove potential leftovers from other source too
        if (tgt.endsWith(".jpg")) {
            altTgt = altTgt.replace(altTgt.length() - 3, 3, "png");
        } else if (tgt.endsWith(".png")) {
            altTgt = altTgt.replace(altTgt.length() - 3, 3, "jpg");
        }
    }
    if (!(skipExisting && (QFile::exists(tgt) || QFile::exists(altTgt)))) {
        QFile::remove(tgt);
        if (GameEntry::Elem::VIDEO & t) {
            if (config->symlink) {
                // symlink
                if (success = QFile::link(cacheFn, tgt); !success) {
                    qWarning() << "Symlink failed, media entry will be not in "
                                  "game list:"
                               << tgt << "->" << cacheFn;
                }
            } else {
                if (success = QFile::copy(cacheFn, tgt); !success) {
                    qWarning() << "Copy video failed, entry will be not in "
                                  "game list:"
                               << cacheFn << "to" << tgt;
                }
            }
        } else {
            if (t & GameEntry::IMAGE) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
                bool diff = tgt.last(3) != altTgt.last(3);
#else
                bool diff = tgt.right(3) != altTgt.right(3);
#endif
                if (diff)
                    QFile::remove(altTgt);
            }
            QFile fh(tgt);
            if (success = fh.open(QIODevice::WriteOnly); success) {
                fh.write(data);
                fh.close();
            } else {
                qWarning()
                    << "Copy failed, media entry will be not in game list:"
                    << cacheFn << "to" << tgt;
            }
        }
    }
    if (success) {
        qDebug() << "Copied" << t;
    }
    return success;
}

QString Pixl::defaultMimeType(const QString &fn) {
    QString ext = "png";
    if (fn.endsWith("-video"))
        ext = "mp4";
    else if (fn.endsWith("-manual"))
        ext = "pdf";
    qDebug() << "Using failsafe extension" << ext << "for" << fn;
    return ext;
}


QString Pixl::getInputFolder() {
    if(config->inputFolder =="")
        return baseFolder() % config->platform;
    else
        return config->inputFolder;
}

QString Pixl::getGameListFolder() {
    return getInputFolder();
}

QString Pixl::getMediaFolder() {
    return getInputFolder();
}

QString Pixl::getBackcoversFolder() {
    return config->mediaFolder % "/backcovers";
}

QString Pixl::getCoversFolder() {
    return config->mediaFolder % "/box2dfront";
}

QString Pixl::getFanartsFolder() {
    return config->mediaFolder % "/fanart";
}

QString Pixl::getFullcoversFolder() {
    return config->mediaFolder % "/boxtexture";
}

QString Pixl::getManualsFolder() {
    return config->mediaFolder % "/manuals";
}

QString Pixl::getMapsFolder() {
    return config->mediaFolder % "/map";
}

QString Pixl::getMarqueesFolder() {
    return config->mediaFolder % "/marquee";
}

QString Pixl::getScreenshotsFolder() {
    return config->mediaFolder % "/screenshot";
}

QString Pixl::getScreenshottitlesFolder() {
    return config->mediaFolder % "/screenshottittle";
}

QString Pixl::getTexturesFolder() {
    return config->mediaFolder % "/support";
}

QString Pixl::get3dcoversFolder() {
    return config->mediaFolder % "/box3d";
}

QString Pixl::getVideosFolder() {
    return config->mediaFolder % "/videos";
}

QString Pixl::getWheelsFolder() {
    return config->mediaFolder % "/wheel";
}

GameEntry::Types Pixl::supportedMedia() {
    return GameEntry::Types(GameEntry::BACKCOVER | GameEntry::COVER  | GameEntry::FANART | GameEntry::FULLCOVER |
                            GameEntry::MANUAL | GameEntry::MAP | GameEntry::MARQUEE |
                            GameEntry::SCREENSHOT | GameEntry::SCREENSHOTTITLE |
                            GameEntry::TEXTURE | GameEntry::THREEDCOVER |
                            GameEntry::VIDEO | GameEntry::WHEEL);
}
