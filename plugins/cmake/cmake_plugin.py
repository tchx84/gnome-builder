#!/usr/bin/env python3

from gi.repository import GObject, Gio
from gi.repository import Ide

class CMakeBuildSystem(Ide.Object, Ide.BuildSystem, Gio.AsyncInitable):
    project_file = GObject.Property(type=Gio.File)

    def do_init_async(self, io_priorty, cancellable, callback, data):
        task = Gio.Task.new(self, cancellable, callback)

        # This is all done synchronously, doing it in a thread would probably
        # be somewhat ideal although unnecessary at this time.

        try:
            # Maybe this is a CMakeLists.txt file
            if self.props.project_file.get_basename() == 'CMakeLists.txt':
                task.return_boolean(True)
                return

            # Maybe this is a directory with a CMakeLists.txt
            if self.props.project_file.query_file_type() == Gio.FileType.DIRECTORY:
                child = self.props.project_file.get_child('CMakeLists.txt')
                if child.query_exists(None):
                    self.props.project_file = child
                    task.return_boolean(True)
                    return
        except Exception as ex:
            task.return_error(ex)

        raise NotImplemented

    def do_init_finish(self, result):
        return result.propagate_boolean()

    def do_get_priority(self):
        # Priority is used to determine the order of discovery
        return 1000

    def do_get_build_flags_async(self, ifile, cancellable, callback, data):
        # GTask sort of is painful from Python.
        # We can use it to attach some data to return from the finish
        # function though.
        task = Gio.Task.new(self, cancellable, callback)
        task.build_flags = ['-DFOO']
        task.return_boolean(True)

    def do_get_build_flags_finish(self, result):
        if task.propagate_boolean():
            return result.build_flags
        raise RuntimeError

    def do_get_builder(self, config):
        return CMakeBuilder(config)

class CMakeBuilder(Ide.Builder):
    def __init__(self, config, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.config = config

    def do_build_async(self, flags, cancellable, callback, data):
        task = Gio.Task.new(self, cancellable, callback)
        task.build_result = CMakeBuildResult(self.config, flags)

        def wrap_execute():
            try:
                task.build_result.execute()
                task.return_boolean(True)
            except Exception as ex:
                task.return_error(ex)

        thread = threading.Thread(target=wrap_execute)
        thread.start()

    def do_build_finish(self, result):
        return result.build_result

class CMakeBuildResult(Ide.BuildResult):
    def __init__(self, config, flags, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.flags = flags

        # execute() runs in a thread, so probably want to extract what
        # you need from the configuration here in the main loop.
        self.config = config

    def execute(self):
        print("Do the actual build, this is in a thread")

        self.set_mode('Successful')
        self.set_failed(False)
        self.set_running(False)
