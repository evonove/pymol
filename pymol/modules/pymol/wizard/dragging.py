from pymol.wizard import Wizard
from pymol import cmd
import pymol
import types
import threading

drag_sele = "_drag"

#def delayed_disable(name,delay=1.0):
#    import time
#    time.sleep(delay)
#    from pymol import cmd
#    cmd.disable(name)
    
class Dragging(Wizard):

    def __init__(self,*arg,**kw):
        _self = kw.get('_self',cmd)
        self.valid = 1
        Wizard.__init__(self,_self)
        if len(arg):
            self.old_button_mode = int(arg[0])
        self.check_valid()
        if self.valid:
            self.recount()

    def recount(self):
        if self.check_valid():
            self.atom_count = self.cmd.count_atoms(drag_sele)
            if self.atom_count:
                obj_list = self.cmd.get_object_list(drag_sele)
                self.obj = obj_list[0]
                print ' Dragging %s atoms in object "%s".'%(self.atom_count,self.obj)
            else:
                self.obj = self.cmd.get_drag_object_name();
                print ' Dragging whole object "%s".'%self.obj
            self.cmd.refresh_wizard()
#        self.cmd.enable(drag_sele)
#        t = threading.Thread(target=delayed_disable,args=(drag_sele,0.5))
#        t.setDaemon(1)
#        t.start()

    def do_dirty(self):
        if self.valid:
            self.check_valid()
        
    def check_valid(self):
        if (not hasattr(self,"cmd")) or self.cmd == None:
            return 0
        elif self.cmd.get_editor_scheme()!=3:
            if self.valid:
                self.valid = 0
                self.cmd.do("_ cmd.set_wizard()")
                self.cmd.refresh_wizard()
                return 0
        else:
            return 1

    def get_event_mask(self):
        return Wizard.event_mask_pick + Wizard.event_mask_dirty
        
    def set_old_button_mode(self,button_mode):
        self.old_button_mode = button_mode
        
    def indicate(self):
        if self.check_valid():
            if drag_sele in self.cmd.get_names("all",enabled_only=1):
                self.cmd.disable(drag_sele)
            else:
                self.cmd.enable(drag_sele)
            self.cmd.refresh_wizard()

    def cleanup(self):
        if (not hasattr(self,"cmd")) or self.cmd == None:
            return
        self.cmd.drag()
        if drag_sele in self.cmd.get_names("all",enabled_only=1):
            self.cmd.disable(drag_sele)
        if self.old_button_mode != None:
            self.cmd.set("button_mode",self.old_button_mode,quiet=1)
            self.cmd.mouse()
    
    def get_panel(self):
        if self.check_valid():
            if self.atom_count:
                panel = [
                    [ 1, 'Dragging %d atoms in'%self.atom_count, ''],
                    [ 1, 'object "'+self.obj+'"', ''],
                    [ 2, 'Undo (CTRL-Z)', 
                      'cmd.undo()'],
                    [ 2, 'Redo (CTRL-A)', 
                      'cmd.redo()'],
                    [ 2, 'Indicate', 
                      'cmd.get_wizard().indicate()'],
                    [ 2, 'Done', 'cmd.set_wizard()' ]
                    ]
            else:
                panel = [
                    [ 1, 'Dragging matrix for',''],
                    [ 1, 'object "'+self.obj+'"', ''],
                    [ 2, 'Reset', 'cmd.reset(object="%s")'%self.obj],
                    [ 2, 'Done', 'cmd.set_wizard()' ]
                    ]
                
        else:
            panel = None
        return panel



