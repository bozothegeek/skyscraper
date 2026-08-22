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

#ifndef PIXL_H
#define PIXL_H

#include "emulationstation.h"
#include "gameentry.h"

class Pixl : public EmulationStation {
    Q_OBJECT

public:
    Pixl();

    QString getInputFolder() override;
    QString getGameListFolder() override;
    QString getMediaFolder() override;

    QString getBackcoversFolder() override;
    QString getCoversFolder() override;
    QString getFanartsFolder() override;
    QString getFullcoversFolder() override;
    QString getManualsFolder() override;
    QString getMapsFolder() override;
    QString getMarqueesFolder() override;
    QString getScreenshotsFolder() override;
    QString getScreenshottitlesFolder() override;
    QString getTexturesFolder() override;
    QString get3dcoversFolder() override;
    QString getVideosFolder() override;
    QString getWheelsFolder() override;

    bool copyMedia(GameEntry::Types &savedMedia,
                         const QString &baseName,
                         const QString &subPath, GameEntry &game) override;
    void assembleList(QString &finalOutput, QList<GameEntry> &gameEntries) override;

protected:
    QStringList createEsVariantXml(const GameEntry &entry) override;
    GameEntry::Types supportedMedia() override;
    bool addEmptyElement() override { return false; };
    QString createXml(GameEntry &entry) override;

private:
    QString getTargetFilePath(GameEntry::Types t, const QString &baseName,
                                      const QString &subPath, const QString &cacheFn,
                                      QString ext = "") override;
};

#endif // PIXL_H
