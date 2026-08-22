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

#include "gameentry.h"
#include "pathtools.h"
#include "strtools.h"

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

static const QRegularExpression REGEX_OPENELEM =
    QRegularExpression("<(\\w+)[\\s>]");

Pixl::Pixl() {}

static const QString baseFolder() { return QString("/recalbox/share/roms/"); }

void Pixl::assembleList(QString &finalOutput, QList<GameEntry> &gameEntries) {
    QString extensions = platformFileExtensions();
    // Check if the platform has both cue and bin extensions. Remove
    // bin if it does to avoid count() below to be 2. I thought
    // about removing bin extensions entirely from platform.cpp, but
    // I assume I've added them per user request at some point.
    bool cueSuffix = false;
    if (extensions.contains("*.cue")) {
        cueSuffix = true;
        if (extensions.contains("*.bin")) {
            extensions.replace("*.bin", "");
            extensions = extensions.simplified();
        }
    }

    QList<GameEntry> added;
    QDir inputDir = QDir(config->inputFolder);

    for (auto &entry : gameEntries) {
        if (config->platform == "daphne") {
            // 'daphne/roms/yadda_yadda.zip' -> 'daphne/yadda_yadda.daphne'
            entry.path.replace("daphne/roms/", "daphne/")
                .replace(".zip", ".daphne");
            continue;
        }
        if (config->platform == "scummvm") {
            // entry.path is file folder on fs with valid extension -> keep as
            // game entry
            // RetroPie/roms/scummvm/blarf.svm/ -> as <game/>
            QFileInfo entryInfo(entry.path);
            if (entryInfo.isDir() &&
                extensions.contains("*." % entryInfo.suffix().toLower())) {
                qDebug()
                    << entry.path
                    << "marked as <game/> albeit being a filesystem folder";
                continue;
            }
        }
        QFileInfo entryInfo(entry.path);
        // always use absolute file path to ROM
        entry.path = entryInfo.absoluteFilePath();

               // Check if path is exactly one subfolder beneath root platform
               // folder (has one more '/') and uses *.cue suffix
        QString entryDir = entryInfo.absolutePath();
        if (cueSuffix &&
            entryDir.count("/") == config->inputFolder.count("/") + 1) {
            // Check if subfolder has exactly one ROM, in which case we
            // use <folder>
            if (QDir(entryDir, extensions).count() == 1) {
                entry.isFolder = true;
                entry.path = entryDir;
            }
        }

               // inputDir is absolute (cf. Skyscraper::run())
        QString subPath = inputDir.relativeFilePath(entryDir);
        if (subPath != ".") {
            // <folder> element(s) are needed
            addFolder(config->inputFolder, subPath, added);
        }
    }

    gameEntries.append(added);

    int dots = -1;
    int dotMod = 1 + gameEntries.length() * 0.1;

    finalOutput.append("<?xml version=\"1.0\"?>\n" /* TODO: xmlPreamble() per frontend resp. with flag for ' encoding="UTF-8"' */);
    finalOutput.append(taintGamelist());
    finalOutput.append("<gameList>\n");
    finalOutput.append("  <provider>\n");
    finalOutput.append(QString("    <System>%1</System>\n").arg(config->platform));
    finalOutput.append("    <software>skyscraper</software>\n");
    finalOutput.append(QString("    <database>%1</database>\n").arg(config->scraper));
    finalOutput.append("</provider>\n");
    for (auto &entry : gameEntries) {
        if (++dots % dotMod == 0) {
            ncprintf(".");
            fflush(stdout);
        }

        if (entry.isFolder && !config->addFolders &&
            !existingInGamelist(entry)) {
            qDebug() << "addFolders is false, directory not added (but may be "
                        "preserved): "
                     << entry.path;
            continue;
        }

        preserveFromOld(entry);

        if (config->relativePaths) {
            entry.path = "./" + PathTools::lexicallyRelativePath(
                                    config->inputFolder, entry.path);
        }
        finalOutput.append(createXml(entry));
    }
    finalOutput.append("</gameList>\n");
}


QString Pixl::createXml(GameEntry &entry) {
    QStringList l;
    bool addEmptyElem = addEmptyElement() && !entry.isFolder;
    l.append(openingElement(entry));

    l.append(elem("path", entry.path, addEmptyElem));
    l.append(elem(GameEntry::getTag(GameEntry::Elem::TITLE), entry.title,
                  addEmptyElem));

    l += createEsVariantXml(entry);

    l.append(elem(GameEntry::getTag(GameEntry::Elem::RATING), entry.rating,
                  addEmptyElem));
    l.append(elem(GameEntry::getTag(GameEntry::Elem::DESCRIPTION),
                  StrTools::shortenText(entry.description, config->maxLength),
                  addEmptyElem));

    QString released = entry.releaseDate;
    QRegularExpressionMatch m = isoTimeRe().match(released);
    if (!m.hasMatch()) {
        released = released % "T000000";
    }
    l.append(elem(GameEntry::getTag(GameEntry::Elem::RELEASEDATE), released,
                  addEmptyElem));

    l.append(elem(GameEntry::getTag(GameEntry::Elem::DEVELOPER),
                  entry.developer, addEmptyElem));
    l.append(elem(GameEntry::getTag(GameEntry::Elem::PUBLISHER),
                  entry.publisher, addEmptyElem));
    l.append(elem(GameEntry::getTag(GameEntry::Elem::TAGS), entry.tags,
                  addEmptyElem));
    l.append(elem(GameEntry::getTag(GameEntry::Elem::PLAYERS), entry.players,
                  addEmptyElem));

           // write out non scraped elements
    const QString tagKidgame = GameEntry::getTag(GameEntry::Elem::AGES);
    for (const auto &t : extraGamelistTags(entry.isFolder)) {
        if (t != tagKidgame) {
            l.append(elem(t, entry.getEsExtra(t), false));
        }
    }
    QString kidGame = entry.getEsExtra(tagKidgame);
    if (kidGame.isEmpty() && entry.ages.toInt() >= 1 &&
        entry.ages.toInt() <= 10) {
        kidGame = "true";
    }

    l.append(elem(tagKidgame, kidGame, false));

    QString outerElemName = REGEX_OPENELEM.match(l[0]).captured(1);
    l.append(QString(INDENT % "</%1>").arg(outerElemName));
    l.removeAll("");

    return l.join("\n") % "\n";
}

QStringList Pixl::createEsVariantXml(const GameEntry &entry) {
    QStringList l;
    bool addEmptyElem = addEmptyElement() && !entry.isFolder;

    // BACKCOVER
    if (!config->xmlTagBackcovers.isEmpty()) {
        if (config->xmlTagBackcovers != "false") {
            l.append(elem(config->xmlTagBackcovers, entry.backcoverFile, addEmptyElem, true));
        }
    } else {
        l.append(elem(GameEntry::getTag(GameEntry::Elem::BACKCOVER), entry.backcoverFile, addEmptyElem, true));
    }

    // COVER
    if (!config->xmlTagCovers.isEmpty()) {
        if (config->xmlTagCovers != "false") {
            l.append(elem(config->xmlTagCovers, entry.coverFile, addEmptyElem, true));
        }
    } else {
        l.append(elem(GameEntry::getTag(GameEntry::Elem::COVER), entry.coverFile, addEmptyElem, true));
    }

    // FANART
    if (!config->xmlTagFanarts.isEmpty()) {
        if (config->xmlTagFanarts != "false") {
            l.append(elem(config->xmlTagFanarts, entry.fanartFile, addEmptyElem, true));
        }
    } else {
        l.append(elem(GameEntry::getTag(GameEntry::Elem::FANART), entry.fanartFile, addEmptyElem, true));
    }

    // FULLCOVER
    if (!config->xmlTagFullcovers.isEmpty()) {
        if (config->xmlTagFullcovers != "false") {
            l.append(elem(config->xmlTagFullcovers, entry.fullcoverFile, addEmptyElem, true));
        }
    } else {
        l.append(elem(GameEntry::getTag(GameEntry::Elem::FULLCOVER), entry.fullcoverFile, addEmptyElem, true));
    }

    // MANUAL
    if (!config->xmlTagManuals.isEmpty()) {
        if (config->xmlTagManuals != "false") {
            l.append(elem(config->xmlTagManuals, entry.manualFile, addEmptyElem, true));
        }
    } else {
        l.append(elem(GameEntry::getTag(GameEntry::Elem::MANUAL), entry.manualFile, addEmptyElem, true));
    }

    // MAP
    if (!config->xmlTagMaps.isEmpty()) {
        if (config->xmlTagMaps != "false") {
            l.append(elem(config->xmlTagMaps, entry.mapFile, addEmptyElem, true));
        }
    } else {
        l.append(elem(GameEntry::getTag(GameEntry::Elem::MAP), entry.mapFile, addEmptyElem, true));
    }

    // MARQUEE
    if (!config->xmlTagMarquees.isEmpty()) {
        if (config->xmlTagMarquees != "false") {
            l.append(elem(config->xmlTagMarquees, entry.marqueeFile, addEmptyElem, true));
        }
    } else {
        l.append(elem(GameEntry::getTag(GameEntry::Elem::MARQUEE), entry.marqueeFile, addEmptyElem, true));
    }

    // SCREENSHOT
    if (!config->xmlTagScreenshots.isEmpty()) {
        if (config->xmlTagScreenshots != "false") {
            l.append(elem(config->xmlTagScreenshots, entry.screenshotFile, addEmptyElem, true));
        }
    } else {
        l.append(elem(GameEntry::getTag(GameEntry::Elem::SCREENSHOT), entry.screenshotFile, addEmptyElem, true));
    }

    // SCREENSHOTTITLE
    if (!config->xmlTagScreenshottitles.isEmpty()) {
        if (config->xmlTagScreenshottitles != "false") {
            l.append(elem(config->xmlTagScreenshottitles, entry.screenshottitleFile, addEmptyElem, true));
        }
    } else {
        l.append(elem(GameEntry::getTag(GameEntry::Elem::SCREENSHOTTITLE), entry.screenshottitleFile, addEmptyElem, true));
    }

    // TEXTURE
    if (!config->xmlTagTextures.isEmpty()) {
        if (config->xmlTagTextures != "false") {
            l.append(elem(config->xmlTagTextures, entry.textureFile, addEmptyElem, true));
        }
    } else {
        l.append(elem(GameEntry::getTag(GameEntry::Elem::TEXTURE), entry.textureFile, addEmptyElem, true));
    }

    // THREEDCOVER
    if (!config->xmlTag3dcovers.isEmpty()) {
        if (config->xmlTag3dcovers != "false") {
            l.append(elem(config->xmlTag3dcovers, entry.threedcoverFile, addEmptyElem, true));
        }
    } else {
        l.append(elem(GameEntry::getTag(GameEntry::Elem::THREEDCOVER), entry.threedcoverFile, addEmptyElem, true));
    }

    // VIDEO
    if (!config->xmlTagVideos.isEmpty()) {
        if (config->xmlTagVideos != "false") {
            l.append(elem(config->xmlTagVideos, entry.videoFile, addEmptyElem, true));
        }
    } else {
        l.append(elem(GameEntry::getTag(GameEntry::Elem::VIDEO), entry.videoFile, addEmptyElem, true));
    }

    // WHEEL
    if (!config->xmlTagWheels.isEmpty()) {
        if (config->xmlTagWheels != "false") {
            l.append(elem(config->xmlTagWheels, entry.wheelFile, addEmptyElem, true));
        }
    } else {
        l.append(elem(GameEntry::getTag(GameEntry::Elem::WHEEL), entry.wheelFile, addEmptyElem, true));
    }

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

    if ((!config->backcovers && !config->cacheBackcovers) || (config->xmlTagBackcovers == "false"))
        toCopy ^= GameEntry::BACKCOVER;

    if (!config->cacheCovers || (config->xmlTagCovers == "false"))
        toCopy ^= GameEntry::COVER;

    if ((!config->fanart && !config->cacheFanarts) || (config->xmlTagFanarts == "false"))
        toCopy ^= GameEntry::FANART;

    if (!config->cacheFullcovers || (config->xmlTagFullcovers == "false"))
        toCopy ^= GameEntry::FULLCOVER;

    if ((!config->manuals && !config->cacheManuals) || (config->xmlTagManuals == "false"))
        toCopy ^= GameEntry::MANUAL;

    if (!config->cacheMaps || (config->xmlTagMaps == "false"))
        toCopy ^= GameEntry::MAP;

    if (!config->cacheMarquees || (config->xmlTagMarquees == "false"))
        toCopy ^= GameEntry::MARQUEE;

    if (!config->cacheScreenshots || (config->xmlTagScreenshots == "false"))
        toCopy ^= GameEntry::SCREENSHOT;

    if (!config->cacheScreenshottitles || (config->xmlTagScreenshottitles == "false"))
        toCopy ^= GameEntry::SCREENSHOTTITLE;

    if (!config->cacheTextures || (config->xmlTagTextures == "false"))
        toCopy ^= GameEntry::TEXTURE;

    if (!config->cache3dcovers || (config->xmlTag3dcovers == "false"))
        toCopy ^= GameEntry::THREEDCOVER;

    if ((!config->videos && !config->cacheVideos) || (config->xmlTagVideos == "false"))
        toCopy ^= GameEntry::VIDEO;

    if (!config->cacheWheels || (config->xmlTagWheels == "false"))
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
