/* This file is part of the KDE project
 * Copyright (C) 2009 Jan Hambrecht <jaham@gmx.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef KOSHAPETRANSPARENCYCOMMAND_H
#define KOSHAPETRANSPARENCYCOMMAND_H

#include "kritaflake_export.h"

#include <kundo2command.h>
#include <QList>

class KoShape;

/// The undo / redo command for setting the shape transparency
class KRITAFLAKE_EXPORT KoShapeTransparencyCommand : public KUndo2Command
{
public:
    /**
     * Command to set a new shape transparency.
     * @param shapes a set of all the shapes that should get the new background.
     * @param transparency the new shape transparency
     * @param parent the parent command used for macro commands
     */
    KoShapeTransparencyCommand(const QList<KoShape*> &shapes, qreal transparency, KUndo2Command *parent = 0);

    /**
     * Command to set a new shape transparency.
     * @param shape a single shape that should get the new transparency.
     * @param transparency the new shape transparency
     * @param parent the parent command used for macro commands
     */
    KoShapeTransparencyCommand(KoShape *shape, qreal transparency, KUndo2Command *parent = 0);

    /**
     * Command to set new shape transparencies.
     * @param shapes a set of all the shapes that should get a new transparency.
     * @param fills the new transparencies, one for each shape
     * @param parent the parent command used for macro commands
     */
    KoShapeTransparencyCommand(const QList<KoShape*> &shapes, const QList<qreal> &transparencies, KUndo2Command *parent = 0);

    ~KoShapeTransparencyCommand() override;
    /// redo the command
    void redo() override;
    /// revert the actions done in redo
    void undo() override;

    int id() const override;
    bool mergeWith(const KUndo2Command *command) override;

private:
    class Private;
    Private * const d;
};

#endif // KOSHAPETRANSPARENCYCOMMAND_H
