/*
 *  Copyright (c) 2012 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __KIS_SHORTCUT_MATCHER_H
#define __KIS_SHORTCUT_MATCHER_H

#include "kis_abstract_shortcut.h"

#include <QList>
#include "kis_single_action_shortcut.h"

class QMouseEvent;
class QKeyEvent;
class QWheelEvent;

class KisStrokeShortcut;
class KisAbstractInputAction;

/**
 * The class that manages connections between shortcuts and actions.
 *
 * It processes input events and generates state transitions for the
 * actions basing on the data, represented by the shortcuts.
 *
 * The class works with two types of actions: long running
 * (represented by KisStrokeShortcuts) and "atomic"
 * (KisSingleActionShortcut). The former one invole some long
 * interaction with the user by means of a mouse cursor or a tablet,
 * the latter one simple action like "Zoom 100%" or "Reset Rotation".
 *
 * The single action shortcuts are handled quite easily. The matcher
 * listens to the events coming, manages two lists of the pressed keys
 * and buttons and when their content corresponds to some single
 * action shortcut it just runs this shortcut once.
 *
 * The strategy for handling the stroke shortcuts is a bit more
 * complex.  Each such action may be in one of the three states:
 *
 * Idle <-> Ready <-> Running
 *
 * In "Idle" state the action is completely inactive and has no access
 * to the user
 *
 * When the action is in "Ready" state, it means that all the
 * modifiers for the action are already pressed and we are only
 * waiting for a user to press the mouse button and start a stroke. In
 * this state the action can show the user its Cursor to notify him
 * what is going to happen next.
 *
 * In the "Running" state, the action has full access to the user
 * input and is considered to perform all the work it was created for.
 *
 * To implement such state transitions for the actions,
 * KisShortcutMatcher first forms a list of the actions which can be
 * moved to a ready state (m_d->readyShortcuts), then chooses the one
 * with the highest priority to be the only shortcut in the "Ready"
 * state and activates it (m_d->readyShortcut).  Then when the user
 * presses the mouse button, the matcher looks through the list of
 * ready shortcuts, chooses which will be running now, deactivates (if
 * needed) currently activated action and starts the chosen one.
 *
 * \see KisSingleActionShortcut
 * \see KisStrokeShortcut
 */
class KRITAUI_EXPORT KisShortcutMatcher
{
public:
    KisShortcutMatcher();
    ~KisShortcutMatcher();

    void addShortcut(KisSingleActionShortcut *shortcut);
    void addShortcut(KisStrokeShortcut *shortcut);
    void addAction(KisAbstractInputAction *action);

    /**
     * Handles a key press event.
     *
     * \return whether the event has been handled successfully and
     * should be eaten by the events filter
     */
    bool keyPressed(Qt::Key key);

    /**
     * Handles a key release event.
     *
     * \return whether the event has been handled successfully and
     * should be eaten by the events filter
     */
    bool keyReleased(Qt::Key key);

    /**
     * Handles the mouse button press event
     *
     * \param button the button that has been pressed
     * \param event the event that caused this call
     *
     * \return whether the event has been handled successfully and
     * should be eaten by the events filter
     */
    bool buttonPressed(Qt::MouseButton button, QMouseEvent *event);

    /**
     * Handles the mouse button release event
     *
     * \param button the button that has been pressed
     * \param event the event that caused this call
     *
     * \return whether the event has been handled successfully and
     * should be eaten by the events filter
     */
    bool buttonReleased(Qt::MouseButton button, QMouseEvent *event);

    /**
     * Handles the mouse wheel event
     *
     * \return whether the event has been handled successfully and
     * should be eaten by the events filter
     */
    bool wheelEvent(KisSingleActionShortcut::WheelAction wheelAction, QWheelEvent *event);

    /**
     * Handles the mouse move event
     *
     * \param event the event that caused this call
     *
     * \return whether the event has been handled successfully and
     * should be eaten by the events filter
     */
    bool mouseMoved(QMouseEvent *event);

    /**
     * Resets the internal state of the matcher
     *
     * This should be done when the window has lost the focus for
     * some time, so that several events could be lost
     */
    void reset();

    /**
     * Disables the start of any actions.
     *
     * WARNING: the actions that has been started before this call
     * will *not* be ended. They will be ended in their usual way,
     * when the mouse button will be released.
     */
    void suppressAllActions(bool value);

private:
    friend class KisInputManagerTest;

    bool tryRunSingleActionShortcut(Qt::Key key, QKeyEvent *event);
    bool tryRunWheelShortcut(KisSingleActionShortcut::WheelAction wheelAction, QWheelEvent *event);
    template<typename T, typename U> bool tryRunSingleActionShortcutImpl(T param, U *event);

    void prepareReadyShortcuts();

    bool tryRunReadyShortcut(Qt::MouseButton button, QMouseEvent *event);
    void tryActivateReadyShortcut();
    bool tryEndRunningShortcut(Qt::MouseButton button, QMouseEvent *event);

private:
    class Private;
    Private * const m_d;
};

#endif /* __KIS_SHORTCUT_MATCHER_H */
