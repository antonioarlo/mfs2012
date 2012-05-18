from gi.repository import Gtk
import constants
from preferencesdialog import PreferencesDialog
from feed_server import FeedServer

MENU_UI = '''
<ui>
    <menubar name="MenuBar">
        <menu action="TraktorMenu">
            <menuitem action="Preferences" />
            <menuitem action="About" />
            <separator />
            <menuitem action="Quit" />
        </menu>
    </menubar>
</ui>
'''

class TraktorWindow(Gtk.Window):

    def __init__(self):
        Gtk.Window.__init__(self)
        self.set_title('Traktor')
        self.set_size_request(500, 300)

        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        self.add(box)

        self.ui_manager = self._setup_ui_manager()
        menu_bar = self.ui_manager.get_widget('/MenuBar')
        box.pack_start(menu_bar, False, False, 0)

        search_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL)
        box.pack_start(search_box, False, False, 0)

        self.program_combo = self._get_combo()
        search_box.pack_start(self.program_combo, False, False, 0)
        program_entry = self._get_entry()
        search_box.pack_start(program_entry, False, False, 0)


        self.store = Gtk.ListStore(int, str, str)

        view = Gtk.TreeView(self.store)
        renderer = Gtk.CellRendererText()
        column = Gtk.TreeViewColumn('Title', renderer, text=1)
        view.append_column(column)

        renderer = Gtk.CellRendererText()
        column = Gtk.TreeViewColumn('Description', renderer, text=2)
        view.append_column(column)
        view.connect('row-activated', self._on_row_activated)

        box.add(view)

        self._update_list()

        self.connect('delete-event', self._quit)

    def _update_list(self):
        self.store.append([0, 'Cars', 'A red car...'])
        self.store.append([1, 'Toy Story', 'Buzz and Buddy...'])
        self.store.append([2, 'Up', 'A house with baloons...'])


    def _setup_ui_manager(self):
        ui_manager = Gtk.UIManager()
        ui_manager.add_ui_from_string(MENU_UI)
        accel_group = ui_manager.get_accel_group()
        self.add_accel_group(accel_group)
        action_group = Gtk.ActionGroup('Actions')
        action_group.add_actions([
                ('TraktorMenu', None, '_Traktor', None, None, None),
                ('Preferences', Gtk.STOCK_PREFERENCES,
                 '_Preferences', None, 'Key Management',
                 self._on_preferences_action),
                ('About', Gtk.STOCK_ABOUT,
                 '_About', None, 'About this application',
                 self._on_about_action),
                ('Quit', Gtk.STOCK_QUIT,
                 '_Quit', None, 'Quit application',
                 self._on_quit),
                ])
        ui_manager.insert_action_group(action_group)
        return ui_manager

    def _on_about_action(self, action):
        about = Gtk.AboutDialog()
        about.set_program_name("Traktor")
        about.run()
        about.destroy()

    def _on_quit(self, data):
        Gtk.main_quit()

    def _on_row_activated(self, tree_view, path, column):
        item = tree_view.get_model().get_iter(path)
        print 'Title:', tree_view.get_model().get_value(item, 1)

    def _on_preferences_action(self, action):
        preferences = PreferencesDialog(self)
        fs = FeedServer();

        preferences.entry.set_text(fs.apikey_get())
        response=preferences.run()
        if response == Gtk.ResponseType.OK:
            entry_value = preferences.entry.get_text()
            result_set = fs.apikey_set(entry_value) ;
            preferences.destroy()

        elif response == Gtk.ResponseType.CANCEL:
            preferences.destroy()

    def _quit(self, window, event):
        Gtk.main_quit()

    def run(self):
        self.show_all()
        Gtk.main()

    def _get_combo(self):
        tv_programs = [
        (repr(constants.SEARCH_MOVIES_TYPE),'Movies'),
        (repr(constants.SEARCH_SHOWS_TYPE), 'Shows'),
        (repr(constants.SEARCH_EPISODES_TYPE), 'Episodes')]
        program_combo = Gtk.ComboBoxText()
        for tv_program in tv_programs:
            program_combo.append(tv_program[0], tv_program[1])
        program_combo.set_active_id(repr(constants.SEARCH_EPISODES_TYPE))
        return program_combo

    def _get_entry(self):
        search_entry = Gtk.Entry()
        search_entry.set_icon_from_stock(Gtk.EntryIconPosition.SECONDARY,
        Gtk.STOCK_FIND)
        search_entry.set_placeholder_text("Search...")
        search_entry.connect("icon-press", self._on_search_icon_pressed)
        search_entry.connect("activate", self._on_search_enter_pressed)
        return search_entry

    def _on_search_icon_pressed(self, program_entry, icon_pos, event):
        self._on_send_query(program_entry.get_text(),
        self.program_combo.get_active_text())

    def _on_search_enter_pressed(self, program_entry):
        self._on_send_query(program_entry.get_text(),
                            int(self.program_combo.get_active_id()))

    def _on_send_query(self, search_text, search_type):
        feed_server = FeedServer()
        feed_server.search(search_text, search_type, self._on_query_response)

    def _on_query_response(self, results):
        for res in results:
            title = res["title"]
            print "%s" % (title)
