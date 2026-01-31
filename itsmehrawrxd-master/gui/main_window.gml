<window id="main_window" title="My Elegant IDE" width="1024" height="768">
    <menu id="main_menu">
        <menu_item label="File">
            <action name="new_file" label="New"/>
            <action name="open_file" label="Open"/>
            <action name="save_file" label="Save"/>
        </menu_item>
        <menu_item label="Build">
            <action name="build_project" label="Build All"/>
            <action name="run_project" label="Run"/>
        </menu_item>
    </menu>
    <panel id="editor_panel" type="vertical">
        <text_editor id="code_editor" font="monospace" size="12"/>
    </panel>
    <panel id="status_bar" type="horizontal">
        <label id="status_text" text="Ready."/>
    </panel>
</window>
