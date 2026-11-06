import Clutter from 'gi://Clutter';
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import Shell from 'gi://Shell';
import St from 'gi://St';
import * as Main from 'resource:///org/gnome/shell/ui/main.js';
import * as PanelMenu from 'resource:///org/gnome/shell/ui/panelMenu.js';
import * as PopupMenu from 'resource:///org/gnome/shell/ui/popupMenu.js';
import {Extension, InjectionManager} from 'resource:///org/gnome/shell/extensions/extension.js';

export default class SovelGlass extends Extension {
    enable() {
        if (Main.sessionMode.isGreeter)
            return;
        this._signals = [];
        this._surfaces = new Map();
        this._menus = new Set();
        this._buttons = new Map();
        this._openMenu = null;
        this._pending = 0;
        this._settings = new Gio.Settings({schema_id: 'org.gnome.shell'});
        this._apps = Shell.AppSystem.get_default();
        this._tracker = Shell.WindowTracker.get_default();
        this._injections = new InjectionManager();
        const extension = this;
        // All GNOME PopupMenu instances, including window context menus.
        this._injections.overrideMethod(PopupMenu.PopupMenu.prototype, 'open', original => function (...args) {
            extension._decorateMenu(this);
            if (extension._openMenu && extension._openMenu !== this)
                extension._openMenu.close();
            extension._openMenu = this;
            return original.call(this, ...args);
        });
        this._glass(Main.panel, false);
        for (const item of Object.values(Main.panel.statusArea)) {
            if (item.menu)
                this._decorateMenu(item.menu);
        }
        this._jared = new PanelMenu.Button(0.5, 'JARED', false);
        this._jared.add_child(new St.Icon({icon_name: 'starred-symbolic', style_class: 'system-status-icon'}));
        this._jared.menu.addAction('JARED status', () => Main.notify('JARED', 'No local JARED service is connected.'));
        this._jared.menu.addAction('Screenshot', () => Main.screenshotUI.open());
        this._jared.connect('enter-event', () => { this._jared.menu.open(); return Clutter.EVENT_PROPAGATE; });
        Main.panel.addToStatusArea('sovel-jared', this._jared, 0, 'right');
        this._title = new St.Label({style_class: 'sovel-app-title', y_align: Clutter.ActorAlign.CENTER});
        Main.panel._leftBox.add_child(this._title);
        this._dock = new St.BoxLayout({style_class: 'sovel-glass sovel-dock', reactive: true});
        Main.layoutManager.addChrome(this._dock, {affectsStruts: false, trackFullscreen: true});
        this._glass(this._dock, false);
        this._library = new St.Button({style_class: 'sovel-dock-button', can_focus: true, accessible_name: 'App Library', child: new St.Icon({icon_name: 'view-app-grid-symbolic', icon_size: 44})});
        this._library.connect('clicked', () => Main.overview.showApps());
        this._dock.add_child(this._library);
        this._connect(this._apps, 'app-state-changed', () => this._queue());
        this._connect(this._apps, 'installed-changed', () => this._queue());
        this._connect(this._settings, 'changed::favorite-apps', () => this._queue());
        this._connect(global.display, 'notify::focus-window', () => {
            this._openMenu?.close();
            this._title.text = this._tracker.focus_app?.get_name() ?? 'Desktop';
        });
        this._connect(Main.layoutManager, 'monitors-changed', () => this._place());
        this._connect(global.stage, 'captured-event', (_actor, event) => {
            if (!this._openMenu?.isOpen)
                return Clutter.EVENT_PROPAGATE;
            if (event.type() === Clutter.EventType.KEY_PRESS && event.get_key_symbol() === Clutter.KEY_Escape)
                this._openMenu.close();
            if (event.type() === Clutter.EventType.BUTTON_PRESS) {
                const [x, y] = event.get_coords();
                const picked = global.stage.get_actor_at_pos(Clutter.PickMode.REACTIVE, x, y);
                if (picked && !this._openMenu.actor.contains(picked) && !this._openMenu.sourceActor?.contains(picked))
                    this._openMenu.close();
            }
            return Clutter.EVENT_PROPAGATE;
        });
        this._title.text = this._tracker.focus_app?.get_name() ?? 'Desktop';
        this._sync();
    }
    _connect(object, signal, callback) {
        const wrapped = signal === 'destroy' ? (...args) => {
            this._signals = this._signals.filter(([target]) => target !== object);
            callback(...args);
        } : callback;
        this._signals.push([object, object.connect(signal, wrapped)]);
    }
    _glass(actor, popup) {
        if (!actor || this._surfaces.has(actor))
            return;
        const effect = new Shell.BlurEffect({mode: Shell.BlurMode.BACKGROUND, radius: 24, brightness: 0.92});
        actor.add_effect_with_name('sovel-live-backdrop', effect);
        actor.add_style_class_name(popup ? 'sovel-popup' : 'sovel-glass');
        this._surfaces.set(actor, {effect, popup});
        // Remove destroyed actors from our ownership map; no redraw polling.
        this._connect(actor, 'destroy', () => this._surfaces.delete(actor));
    }
    _decorateMenu(menu) {
        if (this._menus.has(menu))
            return;
        this._menus.add(menu);
        this._glass(menu.actor, true);
        this._connect(menu, 'open-state-changed', (_menu, open) => {
            if (open) {
                if (this._openMenu && this._openMenu !== menu)
                    this._openMenu.close();
                this._openMenu = menu;
            } else if (this._openMenu === menu) {
                this._openMenu = null;
            }
        });
        this._connect(menu, 'destroy', () => this._menus.delete(menu));
    }
    _queue() {
        if (this._pending)
            return;
        this._pending = GLib.idle_add(GLib.PRIORITY_DEFAULT_IDLE, () => {
            this._pending = 0;
            this._sync();
            return GLib.SOURCE_REMOVE;
        });
    }
    _sync() {
        const favorites = this._settings.get_strv('favorite-apps');
        const ids = [...new Set([...favorites, ...this._apps.get_running().map(app => app.get_id())])];
        for (const [id, button] of this._buttons) {
            if (!ids.includes(id)) {
                this._buttons.delete(id);
                button.destroy();
            }
        }
        let position = 0;
        for (const id of ids) {
            const app = this._apps.lookup_app(id);
            if (!app)
                continue;
            let button = this._buttons.get(id);
            if (!button) {
                button = new St.Button({style_class: 'sovel-dock-button', reactive: true, can_focus: true, accessible_name: app.get_name(), child: app.create_icon_texture(44)});
                button.connect('clicked', () => {
                    const windows = app.get_windows().filter(w => !w.skip_taskbar).sort((a, b) => b.get_user_time() - a.get_user_time());
                    if (!windows.length)
                        app.activate();
                    else if (windows[0].has_focus())
                        windows[0].minimize();
                    else {
                        windows[0].unminimize();
                        Main.activateWindow(windows[0]);
                    }
                });
                button.connect('button-press-event', (_actor, event) => {
                    if (event.get_button() !== 3)
                        return Clutter.EVENT_PROPAGATE;
                    this._appMenu(button, app);
                    return Clutter.EVENT_STOP;
                });
                button.connect('notify::hover', () => button.ease({scale_x: button.hover ? 1.12 : 1, scale_y: button.hover ? 1.12 : 1, duration: 130, mode: Clutter.AnimationMode.EASE_OUT_CUBIC}));
                this._buttons.set(id, button);
                this._dock.insert_child_at_index(button, position);
            }
            this._dock.set_child_at_index(button, position++);
            if (app.get_windows().length)
                button.add_style_class_name('sovel-running');
            else
                button.remove_style_class_name('sovel-running');
        }
        this._dock.set_child_above_sibling(this._library, null);
        const width = Math.max(72, position * 62 + 80);
        const monitor = Main.layoutManager.primaryMonitor;
        this._dock.ease({width: Math.min(width, monitor.width - 24), x: monitor.x + (monitor.width - Math.min(width, monitor.width - 24)) / 2, duration: 220, mode: Clutter.AnimationMode.EASE_OUT_CUBIC});
        this._place(false);
    }
    _place(resetWidth = true) {
        const monitor = Main.layoutManager.primaryMonitor;
        if (!monitor)
            return;
        this._dock.set_position(resetWidth ? monitor.x + (monitor.width - this._dock.width) / 2 : this._dock.x, monitor.y + monitor.height - 88);
    }
    _appMenu(button, app) {
        if (this._context) {
            this._context.destroy();
            this._context = null;
        }
        const menu = new PopupMenu.PopupMenu(button, 0.5, St.Side.BOTTOM);
        this._context = menu;
        Main.uiGroup.add_child(menu.actor);
        const id = app.get_id();
        const favorites = this._settings.get_strv('favorite-apps');
        const index = favorites.indexOf(id);
        menu.addAction(index < 0 ? 'Pin to Dock' : 'Unpin from Dock', () => {
            this._settings.set_strv('favorite-apps', index < 0 ? [...favorites, id] : favorites.filter(value => value !== id));
        });
        for (const [label, direction] of [['Move Left', -1], ['Move Right', 1]]) {
            if (index >= 0)
                menu.addAction(label, () => {
                    const next = Math.max(0, Math.min(favorites.length - 1, index + direction));
                    [favorites[index], favorites[next]] = [favorites[next], favorites[index]];
                    this._settings.set_strv('favorite-apps', favorites);
                });
        }
        for (const window of app.get_windows())
            menu.addAction(window.get_title(), () => { window.unminimize(); Main.activateWindow(window); });
        menu.open();
    }
    disable() {
        this._openMenu?.close();
        this._injections?.clear();
        for (const [actor, {effect, popup}] of this._surfaces ?? []) {
            actor.remove_effect(effect);
            actor.remove_style_class_name(popup ? 'sovel-popup' : 'sovel-glass');
        }
        this._surfaces?.clear();
        for (const [object, id] of this._signals ?? []) {
            try { object.disconnect(id); } catch { /* Actor already destroyed. */ }
        }
        this._signals = [];
        if (this._pending)
            GLib.source_remove(this._pending);
        this._jared?.destroy();
        this._context?.destroy();
        this._title?.destroy();
        if (this._dock) {
            Main.layoutManager.removeChrome(this._dock);
            this._dock.destroy();
        }
        this._menus?.clear();
        this._buttons?.clear();
    }
}
