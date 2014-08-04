
import sys, string
import re
import threading
import os
import time

from Tkinter import *
from tkFileDialog import *
import tkMessageBox

import Pmw

from pymol.wizard import cleanup

from pmg_tk.Setting import Setting
from pmg_tk.SetEditor import SetEditor
from pmg_tk.ColorEditor import ColorEditor

from pmg_tk.skins import PMGSkin
from builder import Builder

import traceback

def _darwin_browser_open(url):
    os.popen("open "+url,'r').read()
    
def darwin_browser_open(url):
    t = threading.Thread(target=_darwin_browser_open,args=(url,))
    t.setDaemon(1)
    t.start()

def _doAsync(self_cmd,cmmd,dirty=0):
    self_cmd.do(cmmd) # force strict ordering of commands
    if dirty:
        self_cmd.dirty()

def _def_ext(ext): # platform-specific default extension handling
    if sys.platform != 'win32': 
        ext = None # default extensions don't work right under X11/Tcl/Tk
    return ext

def _wincheck():
    # disable map_gen for v1.4 release; 
    # map_gen will hit windows in v1.4r1
    #return sys.platform not in ("win32", "cygwin")
    return False


## class askfileopenfilter(askopenfilename):
##     """
##     Subclasses open file dialog to include filename filtering
##     """
##     def __init__(self, initialdir = initdir, filetypes=ftypes, multiple=1):
##         super(askfileopen, self).__init__( initialdir, filetypes, multiple=multiple)
        


class Normal(PMGSkin):

    pad = ' ' # extra space in menus
    
    appname        = 'The PyMOL Molecular Graphics System'
    appversion     = '0.0.0.0' # will be set in __init__
    copyright      = ('Copyright (C) 2003-%d\n' % (time.localtime().tm_year,) +
                      'Schrodinger LLC.\n'+
                      'All rights reserved.')
    contactweb     = 'http://www.pymol.org'
    contactemail   = 'sales@schrodinger.com'
    
    # responsible for setup and takedown of the normal skin

    @property
    def initialdir(self):
        '''
        Be in sync with cd/pwd on the console until the first file has been
        browsed, then remember the last directory.
        '''
        return self._initialdir or os.getcwd()

    @initialdir.setter
    def initialdir(self, value):
        self._initialdir = value

    def complete(self,event):
        st = self.cmd._parser.complete(self.command.get())
        if st:
            self.command.set(st)
            self.entry.icursor(len(st))
        self.focus_entry = 1
        return 1

    def createDataArea(self):
        # Create data area where data entry widgets are placed.
        self.dataArea = self.app.createcomponent('dataarea',
                                             (), None,
                                             Frame, (self.app._hull,), 
                                             relief=SUNKEN, 
                                             bd=1)
        self.dataArea.pack(side=LEFT, fill=BOTH, expand=YES,
                            padx=1, pady=1)

    def destroyDataArea(self):
        self.app.destroycomponent('dataarea')
        
    def createCommandArea(self):
        # Create a command area for application-wide buttons.
        self.commandFrame = self.app.createcomponent('commandframe', (), None,
            Frame,(self.app._hull,),relief=SUNKEN,bd=1)
        self.commandFrame.place(width=500)
        self.commandFrame.pack(side=TOP, 
                         expand=NO, 
                         fill=BOTH,
                         padx=1,
                         pady=1)

    def destroyCommandArea(self):
        self.app.destroycomponent('commandframe')
        
    def createMessageBar(self):
        self.messageBar = Pmw.MessageBar(self.commandFrame, entry_width = 25,
             entry_relief='sunken', entry_borderwidth=1) #, labelpos = 'w')


        self.abortButton=Button(self.commandFrame,
                      text='Abort',highlightthickness=0,
#                                state=DISABLED,
                      command=lambda s=self:self.abort(),padx=0,pady=0)
        self.abortButton.pack(side=RIGHT,fill=BOTH,expand=YES)

        self.abortButton=Button(self.commandFrame,
                                text='Rebuild',highlightthickness=0,
                                #                                state=DISABLED,
                                command=lambda s=self:self.rebuild(),padx=0,pady=0)
        self.abortButton.pack(side=RIGHT,fill=BOTH,expand=YES)

        self.messageBar.pack(side=BOTTOM, anchor=W, fill=X, expand=1)
        self.balloon.configure(statuscommand = self.messageBar.helpmessage)

    def destroyMessageBar(self):
        self.messageBar.destroy()
        
    def get_current_session_file(self):
        session_file = self.cmd.get_setting_text("session_file")        
        session_file = session_file.replace("\\","/") # always use unix-like path separators
        return session_file

    def set_current_session_file(self, session_file):
        session_file = session_file.replace("\\","/") # always use unix-like path separators
        self.cmd.set("session_file",session_file)

    def confirm_quit(self,e=None):
        if int(self.cmd.get_setting_legacy("session_changed")):
            session_file = self.get_current_session_file()
            if session_file != '':
                message = "Save the current session '%s'?"%os.path.split(session_file)[1]
            else:
                message = "Save the current session?"
            check = tkMessageBox._show("Save Session", message,
                                       tkMessageBox.QUESTION, tkMessageBox.YESNOCANCEL)
            if check==tkMessageBox.YES:
                if self.session_save():
                    self.quit_app()
            elif check==tkMessageBox.NO:
                self.quit_app()
        else:
            self.quit_app()

    def quit_app(self):
        self.cmd.log_close()
        self.cmd.quit()  # avoid logging this - it is inconvenient...


    def buttonAdd(self,frame,text,cmmd):
        newBtn=Button(frame,
                      text=text,highlightthickness=0,
                      command=cmmd,padx=0,pady=0)
        newBtn.pack(side=LEFT,fill=BOTH,expand=YES)
        return newBtn

    def get_view(self):
        self.cmd.get_view(quiet=0)
        try:
            str = self.cmd.get_view(3,quiet=1)
            self.root.clipboard_clear()
            self.root.clipboard_append(str)
            self.last_view = str
            self.app.selection_clear()
            self.app.selection_own()
            self.app.selection_handle(lambda a,b,s=self:s.last_view)
            print " PyMOL: Viewing matrix copied to clipboard."
        except:
            traceback.print_exc()
        
    def createButtons(self):
        self.buttonArea = Frame(self.root)
        self.buttonArea.pack(side=TOP, anchor=W)
        
        row1 = self.app.createcomponent('row1', (), None,
            Frame,self.commandFrame,bd=0)
        row1.pack(side=TOP,fill=BOTH,expand=YES)
        btn_reset = self.buttonAdd(row1,'Reset',lambda s=self: s.cmd.do("_ reset"))
        btn_reset = self.buttonAdd(row1,'Zoom',lambda s=self: s.cmd.do("_ zoom animate=-1"))
        btn_orient = self.buttonAdd(row1,'Orient',lambda s=self: s.cmd.do("_ orient animate=1"))        
        btn_rtrace = self.buttonAdd(row1,'Draw',lambda s=self: s.cmd.do("_ draw"))        
        btn_rtrace = self.buttonAdd(row1,'Ray',lambda s=self: s.cmd.do("_ ray async=1"))

        row2 = self.app.createcomponent('row2', (), None,
            Frame,self.commandFrame,bd=0)
        row2.pack(side=TOP,fill=BOTH,expand=YES)
        btn_unpick = self.buttonAdd(row2,'Unpick',lambda s=self: s.cmd.do("_ unpick"))
        btn_hidesele = self.buttonAdd(row2,'Deselect',self.hide_sele)
        btn_reset = self.buttonAdd(row2,'Rock',lambda s=self: s.cmd.do("_ rock"))
        btn_getview = self.buttonAdd(row2,'Get View',lambda s=self: s.get_view()) # doesn't get logged


        row3 = self.app.createcomponent('row3', (), None,
            Frame,self.commandFrame,bd=0)
        row3.pack(side=TOP,fill=BOTH,expand=YES)
        btn_rewind = self.buttonAdd(row3,'|<',lambda s=self: s.cmd.do("_ rewind"))
        btn_back = self.buttonAdd(row3,'<',lambda s=self: s.cmd.do("_ backward"))
        btn_stop = self.buttonAdd(row3,'Stop',lambda s=self: s.cmd.do("_ mstop"))
        btn_play = self.buttonAdd(row3,'Play',lambda s=self: s.cmd.do("_ mplay"))
        btn_forward = self.buttonAdd(row3,'>',lambda s=self: s.cmd.do("_ forward"))
        btn_last = self.buttonAdd(row3,'>|',lambda s=self: s.cmd.do("_ ending"))
        btn_ccache = self.buttonAdd(row3,'MClear',lambda s=self: s.cmd.do("_ mclear"))

        row4 = self.app.createcomponent('row4', (), None,
            Frame,self.commandFrame,bd=0)
        row4.pack(side=TOP,fill=BOTH,expand=YES)
        self.cmdB = self.buttonAdd(row4,'Command',
                                            lambda s=self:
                                            s.toggleFrame(s.cmdFrame))
        self.buildB = self.buttonAdd(row4,'Builder',
                                              lambda s=self:
                                              s.toggleFrame(s.buildFrame))
        self.volB = self.buttonAdd(row4, 'Volume',
                                    self.newVolumeFrame)
        # initialize disabled
        self.volB.config(state=DISABLED)

    def newVolumeFrame(self):
        volumes = self.cmd.get_names_of_type("object:volume", public=1)
        if not volumes:
            return
        if len(volumes) == 1:
            self.cmd.volume_panel(volumes[0])
            return
        def callback():
            sels = listbox.getcurselection()
            if sels:
                self.cmd.volume_panel(sels[0])
            window.destroy()
        title = 'Select a volume object'
        window = Toplevel(self.app.root)
        window.title(title)
        listbox = Pmw.ScrolledListBox(window,
                labelpos='nw',
                label_text=title,
                items=volumes,
                selectioncommand=callback)
        listbox.pack(padx=5, pady=5)
        x, y = window.winfo_pointerxy()
        window.geometry('+%d+%d' % (x - 20, y - 20))

    def destroyButtonArea(self):
        self.app.destroycomponent('row1')
        self.app.destroycomponent('row2')
        self.app.destroycomponent('row3')
        self.app.destroycomponent('row4')
        self.buttonArea.destroy()

    def my_show(self,win,center=1):
        if sys.platform!='linux2':
            win.show()
        else: # autocenter, deiconify, and run mainloop
            # this is a workaround for a bug in the
            # interaction between Tcl/Tk and common Linux
            # window managers (namely KDE/Gnome) which causes
            # an annoying 1-2 second delay in opening windows!
            if center:
                tw = win.winfo_reqwidth()+100
                th = win.winfo_reqheight()+100
                vw = win.winfo_vrootwidth()
                vh = win.winfo_vrootheight()
                x = max(0,(vw-tw)/2)
                y = max(0,(vh-th)/2)
                win.geometry(newGeometry="+%d+%d"%(x,y))
            win.deiconify()
#         win.show()
            
    def my_withdraw(self,win):
        if sys.platform!='linux2':
            win.withdraw()
        else: 
            win.destroy()

    def my_activate(self,win,center=1,focus=None):
        if sys.platform!='linux2':
            win.activate()
        else: # autocenter, deiconify, and run mainloop
            # this is a workaround for a bug in the
            # interaction between Tcl/Tk and common Linux
            # window managers (namely KDE/Gnome) which causes
            # an annoying 1-2 second delay in opening windows!
            if center:
                tw = win.winfo_reqwidth()+100
                th = win.winfo_reqheight()+100
                vw = win.winfo_vrootwidth()
                vh = win.winfo_vrootheight()
                x = max(0,(vw-tw)/2)
                y = max(0,(vh-tw)/2)
                win.geometry(newGeometry="+%d+%d"%(x,y))
            win.deiconify()
            if focus!=None:
                focus.focus_set()
            win.mainloop()
            
    def my_deactivate(self,win):
        if sys.platform!='linux2':
            win.deactivate()
        else: # autocenter, deiconify, and run mainloop
            win.destroy()

    def back_search(self, set0=False):
        if not self.history_cur or set0:
            self.history[0] = self.command.get()
        for i in range(self.history_cur + 1, len(self.history)):
            if self.history[i].startswith(self.history[0]):
                self.history_cur = i
                self.command.set(self.history[self.history_cur])
                l = len(self.history[self.history_cur])
                self.entry.icursor(l)
                break

    def back(self):
        if not self.history_cur:
            self.history[0] = self.command.get()
        self.history_cur = (self.history_cur + 1) & self.history_mask
        self.command.set(self.history[self.history_cur])
        l = len(self.history[self.history_cur])
        self.entry.icursor(l)
    
    def forward(self):
        if not self.history_cur:
            self.history[0] = self.command.get()
        self.history_cur = max(0, self.history_cur - 1) & self.history_mask
        self.command.set(self.history[self.history_cur])
        l = len(self.history[self.history_cur])
        self.entry.icursor(l)

    def doAsync(self,cmmd):
        t = threading.Thread(target=_doAsync,args=(self.cmd,cmmd))
        t.setDaemon(1)
        t.start()
        
    def doTypedCommand(self,cmmd):
        if self.history[1] != cmmd:
            self.history[0]=cmmd
            self.history.insert(0,'') # always leave blank at 0
            self.history.pop(self.history_mask+1)
        self.history_cur = 0
        t = threading.Thread(target=_doAsync,args=(self.cmd,cmmd,1))
        t.setDaemon(1)
        t.start()

    def dump(self,event):
        print dir(event)
        print event.keysym, event.keycode
        
    def createConsole(self):
        self.command = StringVar()      
        self.lineCount = 0
        self.history_mask = 0xFF
        self.history = [''] * (self.history_mask+1)
        self.history_cur = 0

        self.cmdFrame = Frame(self.dataArea)
        self.buildFrame = Builder(self.app, self.dataArea)
        
        self.toggleFrame(self.cmdFrame,startup=1)

        self.entryFrame = Frame(self.cmdFrame)
        self.entryFrame.pack(side=BOTTOM,expand=NO,fill=X)
        self.entry_label = Label(self.entryFrame, text="PyMOL>", padx=1, pady=1, justify=RIGHT)
        self.entry_label.pack(side=LEFT,expand=NO,fill=X)        
        self.entry = Entry(self.entryFrame, justify=LEFT, width=70,
             textvariable=self.command)
        self.entry.pack(side=LEFT,expand=YES,fill=X)
        self.output = Pmw.ScrolledText(self.cmdFrame)
        self.output.pack(side=TOP, fill=BOTH, expand=YES)      

        self.entry.bind('<Return>', lambda e, s=self:
             (s.doTypedCommand(s.command.get()), s.command.set('')))
        self.entry.bind('<Tab>', lambda e, s=self: s.complete(e))
        self.entry.bind('<Up>', lambda e, s=self: s.back())
        self.entry.bind('<Down>', lambda e, s=self: s.forward())
        self.entry.bind('<Control-Up>', lambda e: self.back_search())
        self.root.protocol("WM_DELETE_WINDOW", lambda s=self: s.confirm_quit())
        
        self.log_file = "log.pml"      

#      self.entry = self.app.createcomponent('entry', (), None,
#                           Entry,
#                           (self.dataArea,),
#                           justify=LEFT,
#                           width=50,
###                           textvariable=self.command)

        text = self.output.component('text')
        self.text = text      
        if sys.platform[:5]=='linux':
            text.tk.call('tk','scaling',1)
            if TkVersion < 8.5:
                self.font = 'fixed' # broken by Tk 8.5 / Xft
                self.my_fw_font=(self.font,10)
            else:
                family = 'Bitstream Vera Sans Mono'
                size = 9
                # unfortunately, Tk fonts aren't sized reliably,
                # so here we try to make sure font is actually legible
                import tkFont
                test = tkFont.Font()
                while size<12:
                    test.configure(family=family,size=size)
                    if test.measure("PyMOL")<31:
                        size = size + 1
                    else:
                        break
                self.font = family
                self.my_fw_font=(family,size)
        elif sys.platform[:3]=='win': 
            self.font = 'lucida console' # only available on windows
            self.my_fw_font=(self.font,8) 
        else:
            text.tk.call('tk','scaling',1)
            self.font = 'fixed' # should be available on any X11-based platform
            self.my_fw_font=(self.font,10)

        text.configure(font = self.my_fw_font)
        text.configure(width=74)

        self.balloon.bind(self.entry, 'Command Input Area')
        
        self.focus_entry=0
        self.refocus_entry=0
        if self.app.allow_after:
            self.output.after(100,self.update_feedback)
            self.output.after(100,self.update_menus)
            
        self.output.pack(side=BOTTOM,expand=YES,fill=BOTH)
        self.app.bind(self.entry, 'Command Input Area')

        self.app.bind_all('<F1>',lambda e,s=self: s.cmd.do("cmd._special(1,0,0)"))
        self.app.bind_all('<F2>',lambda e,s=self: s.cmd.do("cmd._special(2,0,0)"))
        self.app.bind_all('<F3>',lambda e,s=self: s.cmd.do("cmd._special(3,0,0)"))
        self.app.bind_all('<F4>',lambda e,s=self: s.cmd.do("cmd._special(4,0,0)"))
        self.app.bind_all('<F5>',lambda e,s=self: s.cmd.do("cmd._special(5,0,0)"))
        self.app.bind_all('<F6>',lambda e,s=self: s.cmd.do("cmd._special(6,0,0)"))
        self.app.bind_all('<F7>',lambda e,s=self: s.cmd.do("cmd._special(7,0,0)"))
        self.app.bind_all('<F8>',lambda e,s=self: s.cmd.do("cmd._special(8,0,0)"))
        self.app.bind_all('<F9>',lambda e,s=self: s.cmd.do("cmd._special(9,0,0)"))
        self.app.bind_all('<F10>',lambda e,s=self: s.cmd.do("cmd._special(10,0,0)"))
        self.app.bind_all('<F11>',lambda e,s=self: s.cmd.do("cmd._special(11,0,0)"))
        self.app.bind_all('<F12>',lambda e,s=self: s.cmd.do("cmd._special(12,0,0)"))
        self.app.bind_all('<Control-F1>',lambda e,s=self: s.cmd.do("cmd._special(1,0,0,2)"))
        self.app.bind_all('<Control-F2>',lambda e,s=self: s.cmd.do("cmd._special(2,0,0,2)"))
        self.app.bind_all('<Control-F3>',lambda e,s=self: s.cmd.do("cmd._special(3,0,0,2)"))
        self.app.bind_all('<Control-F4>',lambda e,s=self: s.cmd.do("cmd._special(4,0,0,2)"))
        self.app.bind_all('<Control-F5>',lambda e,s=self: s.cmd.do("cmd._special(5,0,0,2)"))
        self.app.bind_all('<Control-F6>',lambda e,s=self: s.cmd.do("cmd._special(6,0,0,2)"))
        self.app.bind_all('<Control-F7>',lambda e,s=self: s.cmd.do("cmd._special(7,0,0,2)"))
        self.app.bind_all('<Control-F8>',lambda e,s=self: s.cmd.do("cmd._special(8,0,0,2)"))
        self.app.bind_all('<Control-F9>',lambda e,s=self: s.cmd.do("cmd._special(9,0,0,2)"))
        self.app.bind_all('<Control-F10>',lambda e,s=self: s.cmd.do("cmd._special(10,0,0,2)"))
        self.app.bind_all('<Control-F11>',lambda e,s=self: s.cmd.do("cmd._special(11,0,0,2)"))
        self.app.bind_all('<Control-F12>',lambda e,s=self: s.cmd.do("cmd._special(12,0,0,2)"))                
        
        self.entry.bind('<Prior>',lambda e,s=self: s.cmd.do("cmd._special(104,0,0)"))
        self.entry.bind('<Next>',lambda e,s=self: s.cmd.do("cmd._special(105,0,0)"))
        self.entry.bind('<Control-Prior>',lambda e,s=self: s.cmd.do("cmd._special(104,0,0,2)"))
        self.entry.bind('<Control-Next>',lambda e,s=self: s.cmd.do("cmd._special(105,0,0,2)"))
        self.entry.bind('<Home>',lambda e,s=self: s.cmd.do("cmd._special(106,0,0)"))
        self.entry.bind('<End>',lambda e,s=self: s.cmd.do("cmd._special(107,0,0)"))

    def update_feedback(self):
        if self.focus_entry:
            self.focus_entry=0
            self.entry.focus_set()
        feedback = self.cmd._get_feedback(self.cmd)
        if feedback!=None:
            for a in feedback:
                self.output.insert(END,"\n")
                self.output.insert(END,a)
                self.output.see(END)
                self.lineCount = self.lineCount + 1
                if self.lineCount > 10000:
                    self.output.delete('0.0','%i.%i' % (self.lineCount-5000,0))
                    self.lineCount=5000
#            self.entry.focus_set()
        self.updating = 1
        progress = self.cmd.get_progress()
        if progress>=0.0:
#            self.abortButton.config(state=NORMAL)
            self.messageBar.message("busy","Progress %d%%..."%int(progress*100))
        else:
#            self.abortButton.config(state=DISABLED)            
            self.messageBar.resetmessages("busy")
        if self.app.allow_after:
            if feedback == None: # PyMOL busy, so try more aggressively to get lock
                self.output.after(10,self.update_feedback) # 100X a second                
            else:
                self.output.after(100,self.update_feedback) # 10X a second

    def abort(self):
        self.cmd.interrupt()
#        self.abortButton.config(state=DISABLED)

    def rebuild(self):
        self.doAsync("_ rebuild")
        
    def toggleFrame(self, frame, startup=0):
        if frame not in self.dataArea.slaves():
            # clear all frames in dataArea
            for f in self.dataArea.slaves():
                f.pack_forget()
            # add requested frame to data area
            frame.pack(side=BOTTOM, fill=BOTH, expand=YES)
        else:
            # clear frame from data area
            if frame != self.cmdFrame:
                frame.pack_forget()
                # next command will cause command frame to be turned on if
                # nothing else is visible... might not want this behavior
                self.cmdFrame.pack(side=BOTTOM, fill=BOTH, expand=YES)
                frame = self.cmdFrame
        if not startup:
            if frame == self.cmdFrame:
                if self.edit_mode != None:
                    self.cmd.edit_mode(self.edit_mode)
                    self.edit_mode = None
                if self.auto_overlay != None:
                    self.cmd.set("auto_overlay",self.auto_overlay)
                    self.auto_overlay = None
                if self.valence != None:
                    self.cmd.set("valence",self.valence)
            elif frame == self.buildFrame:
                frame.deferred_activate()
                if "Editing" not in self.cmd.get("button_mode_name"):
                    self.cmd.edit_mode(1)
                    self.edit_mode = 0
                self.valence = self.cmd.get("valence")
                self.cmd.set("valence","1")
                self.auto_overlay = self.cmd.get("auto_overlay")
                self.cmd.set("auto_overlay",1)
            
    def update_menus(self):
        self.setting.refresh()

        if True:
            # volume frame is closed, update the button
            if len(self.cmd.get_names_of_type("object:volume",public=1))>0:
                self.volB.config(state=NORMAL)
            else:
                self.volB.config(state=DISABLED)
        # keep calling
        if self.app.allow_after:
            self.output.after(500,self.update_menus) # twice a second

    def file_open(self,tutorial=0):
        # FIXME: finish
        REFLECTION_FORMATS = ( "MTZ", "mtz", "CIF", "cif" )
        
        if not tutorial:
            initdir = self.initialdir
            ftypes = self.app.getLoadableFileTypes()
        else:
            initdir = os.environ['TUT']
            # only list file extensions that are used for tutorial data
            ftypes = [("Tutorial Data","*.pdb"),]
        if TkVersion>8.3:
            ofile_list = askopenfilename(initialdir = initdir,
                                         filetypes=ftypes,
                                         multiple=1) # new option in Tk 8.4
        else:
            ofile_list = [ askopenfilename(initialdir = initdir,
                                         filetypes=ftypes) ]
        # strange windows bugfix; askopenfiles returns
        # the unicode unparsed tcl list, instead of a Python list
        if ofile_list.__class__==u"".__class__:
            ofile_list = self.app.root.tk.splitlist(ofile_list)

        for ofile in ofile_list:
            if len(ofile):
                if not tutorial:
                    self.initialdir = re.sub(r"[^\/\\]*$","",ofile)
                try:
                    self.cmd.log("load %s\n"%ofile,"cmd.load('%s',quiet=0)\n"%ofile)
                    if (string.lower(ofile[-4:])=='.pse') and (ofile!=self.save_file):
                        self.save_file = '' # remove ambiguous default
                    if ofile[-3:] in REFLECTION_FORMATS and _wincheck():
                        try:
                            from pmg_tk import PyMOLMapLoad
                            map_loader = PyMOLMapLoad.PyMOLMapLoad(self.app.root,self.app,ofile)
                            map_loader.pack_and_show()
                        except:
                            print "Could not load reflection file."
                            traceback.print_exc()
                            return None
                    else:
                        self.cmd.load(ofile,quiet=0)
                    
                except self.pymol.CmdException:
                    print "Error: unable to open file '%s'"%ofile

    def log_open(self):
        sfile = asksaveasfilename(initialfile = self.log_file,
                                  initialdir = self.initialdir,
                                  filetypes=[
            ("PyMOL Script","*.pml"),
            ("PyMOL Program","*.pym"),
            ("Python Program","*.py"),
            ("All Files","*.*"),
            ("All Files","*"),
            ])
        if len(sfile):
            self.initialdir = re.sub(r"[^\/\\]*$","",sfile)
            self.log_file = re.sub(r"^.*[^\/\\]","",sfile)
            self.cmd.log_open(sfile)

    def log_resume(self,append_only=0):
        ofile = askopenfilename(initialdir = os.getcwd(),
                                filetypes=[("All Resumable","*.pml"),
                                           ("All Resumable","*.pym"),
                                           ("All Resumable","*.py"),
                                           ("PyMOL Script","*.pml"),
                                           ("PyMOL Program","*.pym"),
                                           ("Python Program","*.py"),
                                           ("All Files","*.*"),                                           
                                           ("All Files","*"),
                                           ])
        if len(ofile):
            self.initialdir = re.sub(r"[^\/\\]*$","",ofile)
            self.log_file = re.sub(r"^.*[^\/\\]","",ofile)
#            os.chdir(self.initialdir)                 
            self.cmd.resume(ofile)

    def log_append(self,append_only=0):
        ofile = askopenfilename(initialdir = os.getcwd(),
                         filetypes=[("All Appendable","*.pml"),
                                        ("All Appendable","*.pym"),
                                        ("All Appendable","*.py"),
                                        ("PyMOL Script","*.pml"),
                                        ("PyMOL Program","*.pym"),
                                        ("Python Program","*.py"),
                                        ("All Files","*.*"),                                           
                                        ("All Files","*"),
                                        ])
        if len(ofile):
            self.initialdir = re.sub(r"[^\/\\]*$","",ofile)
            self.log_file = re.sub(r"^.*[^\/\\]","",ofile)
#            os.chdir(self.initialdir)                 
            self.cmd.log_open(ofile,'a')

    def session_save(self):
        self.save_file = self.get_current_session_file()
        if self.save_file!='':
            self.cmd.log("save %s,format=pse\n"%(self.save_file),
                      "cmd.save('%s',format='pse')\n"%(self.save_file))
#            self.cmd.save(self.save_file,"","pse",quiet=0)
#            self.cmd.set("session_changed",0)
            self.cmd.do("_ cmd.save('''%s''','','pse',quiet=0)"%self.save_file) # do this in the main thread to block cmd.quit, etc.
            self.cmd.do("_ cmd.set('session_changed',0)")
            return 1
        else:
            return self.session_save_as()

    def session_save_as(self):
        (self.initialdir, self.save_file) = os.path.split(self.get_current_session_file())
        (save_file, def_ext) = os.path.splitext(self.save_file)
        sfile = asksaveasfilename(defaultextension = _def_ext(def_ext),
                                  initialfile = save_file,  
                                  initialdir = self.initialdir,
                                  filetypes=[
            ("PyMOL Session File","*.pse"),
            ("PyMOL Show File","*.psw"),
            ])
        if len(sfile):
            if re.search(r"\.pse$|\.PSE$|\.psw$|\.PSW$",sfile)==None:
                sfile=sfile+".pse"
            self.initialdir = re.sub(r"[^\/\\]*$","",sfile)
            self.cmd.log("save %s,format=pse\n"%(sfile),
                      "cmd.save('%s',format='pse')\n"%(sfile))
#            self.cmd.save(sfile,"",format='pse',quiet=0)
#            self.cmd.set("session_changed",0)
            self.save_file = sfile
#            self.cmd.set("session_file",self.save_file)
            self.set_current_session_file(self.save_file)
            # do this in the main thread to block cmd.quit, etc.
            self.cmd.do("_ cmd.save('''%s''','','pse',quiet=0)"%self.save_file) 
            self.cmd.do("_ cmd.set('session_changed',0)")
            return 1
        else:
            return 0


    def file_save(self):
        """
        File->Save Molecule, now with filtering
        """
        self.dialog = Pmw.SelectionDialog(self.root,
                                          title="Save",
                                          buttons = ('OK', 'Cancel'),
                                          defaultbutton='OK',
                                          scrolledlist_labelpos=N,
                                          scrolledlist_listbox_selectmode=EXTENDED,
                                          label_text='Which object or selection would you like to save?',
                                          scrolledlist_items = (),  # used to be 'lst'
                                          command = self.file_save2)


        self.filter_entry = Pmw.EntryField(self.dialog.interior(),
                                     command=self.filter_names,
                                     labelpos='w',
                                     modifiedcommand=self.update_save_listbox,
                                     validate=None,
                                     value="",
                                     label_text="Filter:")
        self.filter_entry.pack(pady=6, fill='x', expand=0, padx=10)

        self.multiple_files_option = Pmw.RadioSelect( self.dialog.interior(),
                                                      labelpos='w',
                                                      orient='vertical',
                                                      selectmode='single',
                                                      label_text="Save to...",
                                                      buttontype="radiobutton",
                                                      )
        self.multiple_files_option.add("one file")
        self.multiple_files_option.add("multiple files")
        self.multiple_files_option.invoke("one file")
        self.multiple_files_option.pack(side='left', pady=8)
                                                      
                                                 
        self.states_option = Pmw.RadioSelect( self.dialog.interior(),
                                              labelpos='w',
                                              orient='vertical',
                                              selectmode='single',
                                              label_text='Saved state...',
                                              buttontype="radiobutton"
                                              )
        self.states_option.add("all")
        self.states_option.add("global")
        self.states_option.add("object's current")
        self.states_option.invoke("global")
        self.states_option.pack(side='right', pady=8)
                                               
                                                

        # The listbox is created empty.  Fill it now.
        self.update_save_listbox()

        if len(self.dialog.component('scrolledlist').get()):
            # set focus on the first item
            listbox = self.dialog.component('scrolledlist')      
            listbox.selection_set(0)

        self.my_show(self.dialog)
        
    def file_save2(self,result):
        filetypes_save = [
            ("PDB File","*.pdb"),
            ("MOL File","*.mol"),
            ("MOL2 File","*.mol2"),
            ("MMD File","*.mmd"),
            ("PKL File","*.pkl"),
            ("SDF File","*.sdf"),
        ]
        # user hit [CANCEL] button
        if result!='OK':
            self.my_withdraw(self.dialog)
            del self.dialog
        # user hit [OK] button
        else:
            # saves multiple as one file
            sels = self.dialog.getcurselection()

            # save N>1 objects to ONE file
            if self.multiple_files_option.getvalue()=="one file" and len(sels)>=1:
                # save one or more objects to ONE file
                if self.multiple_files_option.getvalue()=="one file":
                    sfile = '_'.join(sels) if len(sels) < 3 else \
                            sels[0] + '-and-%d-more' % (len(sels) - 1)
                    self.my_withdraw(self.dialog)
                    del self.dialog
                    if result=='OK':
                        sfile = asksaveasfilename(defaultextension = _def_ext(".pdb"),
                                                  initialfile = sfile,
                                                  initialdir = self.initialdir,
                                                  filetypes=filetypes_save)
                        if len(sfile):
                            # maybe use PDBSTRs for saving multiple files to multiple states
                            self.initialdir = re.sub(r"[^\/\\]*$","",sfile)
                            save_sele = string.join(map(lambda x:"("+str(x)+")",sels)," or ")
                            self.cmd.log("save %s,(%s)\n"%(sfile,save_sele),
                                         "cmd.save('%s','(%s)')\n"%(sfile,save_sele))
			    if self.states_option.getvalue()=="all":
                                self.cmd.save(sfile,"(%s)"%save_sele,state=0,quiet=0)
                            elif self.states_option.getvalue() == "object's current":
                                ap = 0
                                for sel in sels:
                                    s = int(self.cmd.get("state", str(sel)))
                                    self.cmd.multisave(sfile,str(sel),state=s, quiet=0, append=ap)
                                    ap = 1
			    else:
                                self.cmd.save(sfile,"(%s)"%save_sele,quiet=0)
                            return
            else:
                # save to many files
                self.my_withdraw(self.dialog)
                del self.dialog

                state_flag = self.states_option.getvalue()

                for curName in sels:
                    ## print "Result is: ", result
                    ## print "Sels is: ", sels
                    ## print "CurName is: ", curName
                    ## print "State flag is: ", state_flag

                    # The only special case for saving files is when the user selects a multi-state object
                    # and wants to save that to multiple files, each state in one file.
                    doSplit=False
                    if state_flag=='all':
                        stateSave = "0"
                        if len(sels)==1:
#                            print "User wants to split a file"
                            doSplit=True
                    elif state_flag=='global':
                        stateSave = self.cmd.get_state()
                    elif state_flag=="object's current":
                        stateSave = int(self.cmd.get("state",curName))
#                        print "Saving curren't object's state as: ", stateSave
                    else: # default to current global
                        stateSave = "state=", self.cmd.get_state()

                    if result=='OK':
                        sfile = asksaveasfilename(defaultextension = _def_ext(".pdb"),
                                                  initialfile = curName,
                                                  initialdir = self.initialdir,
                                                  filetypes = filetypes_save)
                        # now save the file (customizing states as necessary)
#                        print "sfile is: ", sfile

                        if len(sfile):
                            # maybe use PDBSTRs for saving multiple files to multiple states
                            self.initialdir = re.sub(r"[^\/\\]*$","",sfile)
                            save_sele = str("("+curName+")")

                            if doSplit:
                                # save each state in "save_sele" to file "sfile" as 'sfile_stateXYZ.pdb'
                                s = self.cmd.count_states(save_sele)
                                for stateSave in range(1,int(s)+1):
                                    save_file = sfile
                                    # _state004
                                    inter = "_state" + string.zfill(str(stateSave), len(str(s))+1)
                                    # g either MATCHES *.pdb or not.  If so, save, name_stateXYZ.pdb
                                    g = re.search("(.*)(\..*)$", save_file)
                                    if g!=None:
                                        # 1PDB_state004.pdb
                                        save_file = g.groups()[0] + inter + g.groups()[1]
                                    else:
                                        # user entered a file w/o an extension name: eg, '1abc'
                                        # this saves to, '1abc_state00XYZ'
                                        save_file = save_file + inter

                                    self.cmd.log("save %s,(%s)\n"%(save_file,save_sele),
                                                 "cmd.save('%s','(%s)', state='%s')\n"%(save_file,save_sele,stateSave))
                                    self.cmd.save(save_file,"(%s)"%save_sele,state=stateSave,quiet=0)
                            else:
                                save_file = sfile

                                # just save current selection to one file
                                self.cmd.log("save %s,(%s)\n"%(save_file,save_sele),
                                             "cmd.save('%s','(%s)', state='%s')\n"%(save_file,save_sele,stateSave))
                                self.cmd.save(save_file,"(%s)"%save_sele,state=stateSave,quiet=0)


    def update_save_listbox(self):
        """
        Update the scrolled list box to represent the filtered
        names after a change to the EntryField is caught
        """
        self.dialog.component("scrolledlist").setlist(self.filter_names())
        

    def filter_names(self):
        """
        Filter names out of the dialog box if they don't match
        """
        #
        # Because this function is a callback for the modified
        # EntryField, it must be FAST
        #
        groups = self.cmd.get_names_of_type("object:group")
        lst = filter(lambda x:x[0]!="_", self.cmd.get_names('all'))

        # if the field for filtering is empty, return all names
        if self.filter_entry.getvalue()=="":
            return lst
        else:
            return filter( lambda x: self.filter_entry.getvalue() in x, lst )

    def hide_sele(self):
        self.cmd.log("util.hide_sele()\n","util.hide_sele()\n")
        self.util.hide_sele()
            
    def edit_pymolrc(self):
        import tkSimpleDialog
        from pmg_tk.TextEditor import TextEditor

        try:
            pymolrc = self.pymol.invocation.options.pymolrc[0]
        except (TypeError, IndexError):
            if sys.platform.startswith('win'):
                pymolrc = os.path.expandvars(r'$HOMEDRIVE$HOMEPATH\pymolrc.pml')
            else:
                pymolrc = os.path.expandvars(r'$HOME/.pymolrc')
            pymolrc = tkSimpleDialog.askstring('Create new pymolrc?',
                    'Filename of new pymolrc',
                    initialvalue=pymolrc)

        if pymolrc:
            TextEditor(self.root, pymolrc, 'pymolrc (%s)' % pymolrc)

    def file_run(self):
        ofile = askopenfilename(initialdir = os.getcwd(),
                         filetypes=[("All Runnable","*.pml"),
                                        ("All Runnable","*.pym"),
                                        ("All Runnable","*.py"),
                                        ("All Runnable","*.pyc"),
                                        ("PyMOL Script","*.pml"),
                                        ("Python Program","*.py"),
                                        ("Python Program","*.pyc"),
                                        ("PyMOL Program","*.pym"),
                                        ("All Files","*.*"),                                           
                                        ("All Files","*"),
                                        ])
        if len(ofile):
            dir = re.sub(r"[^\/\\]*$","",ofile)
            self.__script__ = ofile
#            os.chdir(dir)        
            if re.search("\.pym*$|\.PYM*$",ofile):
                self.cmd.do("run "+ofile);      
            else:
                self.cmd.do("@"+ofile);

    def file_save_png(self):
        sfile = asksaveasfilename(defaultextension = _def_ext(".png"),
                                  initialdir = self.initialdir,
                 filetypes=[("PNG File","*.png")])
        if len(sfile):
            self.initialdir = re.sub(r"[^\/\\]*$","",sfile)
            self.cmd.log("png %s\n"%sfile,"cmd.png('%s')\n"%sfile)
            self.cmd.png(sfile,quiet=0)

    def file_save_wrl(self):
        sfile = asksaveasfilename(defaultextension = _def_ext(".wrl"),
                                  initialdir = self.initialdir,
                 filetypes=[("VRML 2 WRL File","*.wrl")])
        if len(sfile):
            self.initialdir = re.sub(r"[^\/\\]*$","",sfile)
            self.cmd.log("save %s\n"%sfile,"cmd.save('%s')\n"%sfile)
            self.cmd.save(sfile,quiet=0)
            
    def file_save_pov(self):
        sfile = asksaveasfilename(defaultextension = _def_ext(".pov"),
                                  initialdir = self.initialdir,
                                  filetypes=[("POV File","*.pov")])
        if len(sfile):
            self.initialdir = re.sub(r"[^\/\\]*$","",sfile)
            self.cmd.log("save %s\n"%sfile,"cmd.save('%s')\n"%sfile)
            self.cmd.save(sfile,quiet=0)

    def file_save_mpeg(self):
        try:
            from freemol import mpeg_encode
            if not mpeg_encode.validate():
                print "produce-error: Unable to validate freemol.mpeg_encode"
                raise
        except:
            tkMessageBox.showerror("Error",
                "MPEG encoder missing.\nThe FreeMOL add-ons may not be installed.")
            return

        else:
            sfile = asksaveasfilename(defaultextension = _def_ext(".mpg"),
                                      initialdir = self.initialdir,
                                      filetypes=[("MPEG movie file","*.mpg")])
            if len(sfile):
                self.initialdir = re.sub(r"[^\/\\]*$","",sfile)
                mQual = self.cmd.get_setting_int("movie_quality")
                self.cmd.log("movie.produce %s,quality=%d,quiet=0\n"%(sfile,mQual),
                             "cmd.movie.produce('''%s''',quality=%d,quiet=0)\n"%(sfile,mQual))
                self.cmd.movie.produce(sfile,quality=mQual, quiet=0)  #quality=quality
        
    def file_save_mpng(self):
        sfile = asksaveasfilename(initialdir = self.initialdir,
                                  filetypes=[("Numbered PNG Files","*.png")])
        if len(sfile):
            self.initialdir = re.sub(r"[^\/\\]*$","",sfile)
            self.cmd.log("mpng %s\n"%sfile,"cmd.mpng('%s')\n"%sfile)         
            self.cmd.mpng(sfile,modal=-1)

    def mvprg(self, command=None):
        if command != None:
            if command == -1:
                self.cmd.do("_ mdelete -1,%d"%self.movie_start)
                command = None
            else:
                command = str(command)
                self.movie_start = (self.cmd.get_movie_length()+1)
                command = command % self.movie_start
                self.movie_command = command
                self.cmd.do("_ ending")
        else:
            command = self.movie_command
        if command != None:
            self.cmd.do(command)

    def mvprg_scene_loop(self, pause, rock, angle):
        def func():
            cmd = self.cmd
            start = cmd.get_movie_length() + 1
            cmd.ending()
            cmd.set('sweep_angle', angle)
            cmd.movie.add_scenes(None, pause, rock=rock, start=start)
        return func

    def transparency_menu(self,name,label,setting_name):
        
        self.menuBar.addcascademenu('Transparency', name, label, label=label)
        
        for lab, val in [ ('Off', 0.0), ('20%', 0.2), ('40%', 0.4), 
                                ('50%', 0.5), ('60%', 0.6), ('80%', 0.8) ]:
            self.menuBar.addmenuitem(name,  'command', lab, label=lab,
                                             command = lambda v=val,s=self,sn=setting_name: s.cmd.set(sn, v))

    def cat_terms(self):
        for path in [ "$PYMOL_PATH/LICENSE.txt", "$PYMOL_PATH/LICENSE.TXT", "$PYMOL_PATH/LICENSE" ]:
            path = self.pymol.cmd.exp_path(path)
            if os.path.exists(path):
                print string.strip(open(path).read())
                return 
        print " Error: no license terms found."

    def toggleClickThrough(self, toggle):
        if toggle:
            os.system(
            "defaults write com.apple.x11 wm_click_through -bool true")
            os.system(
            "defaults write org.x.X11 wm_click_through -bool true")
            os.system(
            "defaults write com.apple.x11 wm_ffm -bool true")
            os.system(
            "defaults write org.x.X11 wm_ffm -bool true")
            print "Enabled wm_click_through and wm_ffm.",
        else:
            os.system(
            "defaults write com.apple.x11 wm_click_through -bool false")
            os.system(
            "defaults write org.x.X11 wm_click_through -bool false")
            os.system(
            "defaults write com.apple.x11 wm_ffm -bool false")
            os.system(
            "defaults write org.x.X11 wm_ffm -bool false")
            print "Disabled wm_click_through and wm_ffm.",
        print "Please restart X11."

    def createMenuBar(self):
        self.menuBar = Pmw.MenuBar(self.root, balloon=self.balloon,
                                   hull_relief=RAISED, hull_borderwidth=1) 
        self.menuBar.pack(fill=X)

#        self.menuBar.addmenu('Tutorial', 'Tutorial', side='right')      

#        self.menuBar.addmenuitem('Tutorial', 'command', 'Open tutorial data file.',
#                                label='Open File...',
#                                command=lambda s=self: s.file_open(tutorial=1))

# to come
#        self.menuBar.addmenuitem('Tutorial', 'separator', '')
#
#        self.menuBar.addmenuitem('Tutorial', 'command', 'Beginners',
#                                         label='Beginners',
#                                         command = lambda s=self: None)

        self.menuBar.addmenu('Help', 'About %s' % self.appname, side='right')      

        try:
            import webbrowser
            browser_open = webbrowser.open
            
            # workaround for problematic webbrowser module under Mac OS X 
            try:
                if sys.platform == 'darwin':
                    browser_open = darwin_browser_open
            except:
                pass
            
            self.menuBar.addmenuitem('Help', 'command',
                                     'Access the Official PyMOL Documentation online',
                                     label='Online Documentation',
                                     command = lambda bo=browser_open:bo("http://pymol.org/dsc"))


            self.menuBar.addcascademenu('Help', 'Topics', 'Topics',
                                             label='Topics',tearoff=FALSE)

            self.menuBar.addmenuitem('Topics', 'command',
                                     'Introductory Screencasts',
                                     label='Introductory Screencasts',
                                     command = lambda bo=browser_open:bo("http://pymol.org/dsc/id/media:intro"))

            self.menuBar.addmenuitem('Topics', 'command',
                                     'Core Commands',
                                     label='Core Commands',
                                     command = lambda bo=browser_open:bo("http://pymol.org/dsc/id/command:core_set"))

            self.menuBar.addmenuitem('Topics', 'separator', '')

            self.menuBar.addmenuitem('Topics', 'command',
                                     'Settings',
                                     label='Settings',
                                     command = lambda bo=browser_open:bo("http://pymol.org/dsc/id/setting"))

            self.menuBar.addmenuitem('Topics', 'command',
                                     'Atom Selections',
                                     label='Atom Selections',
                                     command = lambda bo=browser_open:bo("http://pymol.org/dsc/id/selection"))
                                    
            self.menuBar.addmenuitem('Topics', 'command',
                                     'Commands',
                                     label='Commands',
                                     command = lambda bo=browser_open:bo("http://pymol.org/dsc/id/command"))
            
            self.menuBar.addmenuitem('Topics', 'command',
                                     'Launching',
                                     label='Launching',
                                     command = lambda bo=browser_open:bo("http://pymol.org/dsc/id/launch"))
            
            self.menuBar.addmenuitem('Topics', 'separator', '')
            
            self.menuBar.addmenuitem('Topics', 'command',
                                     'Concepts',
                                     label='Concepts',
                                     command = lambda bo=browser_open:bo("http://pymol.org/dsc/id/concept"))

            self.menuBar.addmenuitem('Topics', 'separator', '')
            
            self.menuBar.addmenuitem('Topics', 'command',
                                     'A.P.I. Methods',
                                     label='A.P.I. Methods',
                                     command = lambda bo=browser_open:bo("http://pymol.org/dsc/id/api"))

            self.menuBar.addmenuitem('Help', 'separator', '')
            
            self.menuBar.addmenuitem('Help', 'command',
                                     'Access the community-maintained PyMOL Wiki',
                                     label='PyMOL Community Wiki',
                                     command = lambda bo=browser_open:bo("http://www.pymolwiki.org"))

            self.menuBar.addmenuitem('Help', 'command',
                                     'Join or browse the pymol-users mailing list',
                                     label='PyMOL Mailing List',
                                     command = lambda bo=browser_open:bo("https://lists.sourceforge.net/lists/listinfo/pymol-users"))

            self.menuBar.addmenuitem('Help', 'command',
                                     'Access the PyMOL Home Page',
                                     label='PyMOL Home Page',
                                     command = lambda bo=browser_open:bo("http://www.pymol.org"))
            
            self.menuBar.addmenuitem('Help', 'separator', '')
            
            self.menuBar.addmenuitem('Help', 'command',
                                     'Email PyMOL Help',
                                     label='Email PyMOL Help',
                                     command = lambda bo=browser_open:bo("mailto:help@schrodinger.com?subject=PyMOL%20Question"))

            self.menuBar.addmenuitem('Help', 'separator', '')        

            self.menuBar.addmenuitem('Help', 'command',
                                     'Get information on application', 
                                     label='About PyMOL', command = lambda s=self:s.show_about())

            if self.pymol.cmd.splash(2):
                self.menuBar.addmenuitem('Help', 'command',
                                         'Sponsor PyMOL by becoming a Subscriber',
                                         label='Sponsorship Information',
                                         command = lambda bo=browser_open:bo("http://pymol.org/funding.html"))

            self.menuBar.addmenuitem('Help', 'command',
                                     'Learn How to Cite PyMOL', 
                                     label='How to Cite PyMOL', command = lambda bo=browser_open:bo("http://pymol.org/citing"))
            
            #self.menuBar.addmenuitem('Help', 'separator', '')

            #self.menuBar.addmenuitem('Help', 'command',
            #                         'Output License Terms',
            #                         label='Output License Terms',
            #                         command = lambda s=self:s.cat_terms())


        except ImportError:
            pass
        

#      self.menuBar.addmenuitem('Help', 'command', 'Release Notes',
#                               label='Release Notes',
#                               command = lambda s=self: s.cmd.do("_ cmd.show_help('release')"))

#      self.menuBar.addmenuitem('Help', 'separator', '')
        
#      self.menuBar.addmenuitem('Help', 'command', 'Help on Commands',
#                               label='Commands',
#                               command = lambda s=self: s.cmd.do("_ cmd.show_help('commands')"))

#      self.menuBar.addmenuitem('Help', 'command', 'Help on Launching',
#                               label='Launching',
#                               command = lambda s=self: s.cmd.do("_ cmd.show_help('launching')"))      

#      self.menuBar.addmenuitem('Help', 'separator', '')

#      self.menuBar.addmenuitem('Help', 'command', 'Help on Selections',
#                               label='Select Command',
#                               command = lambda s=self: s.cmd.do("_ cmd.show_help('select')"))      

#      self.menuBar.addmenuitem('Help', 'command', 'Help on Selections',
#                               label='Selection Syntax',
#                               command = lambda s=self: s.cmd.do("_ cmd.show_help('selections')"))      

#      self.menuBar.addmenuitem('Help', 'command', 'Example Selections',
#                               label='Selection Examples',
#                               command = lambda s=self: s.cmd.do("_ cmd.show_help('examples')"))      

#      self.menuBar.addmenuitem('Help', 'separator', '')
        

#      self.menuBar.addmenuitem('Help', 'command', 'Help on the Mouse',
#                               label='Mouse',
#                               command = lambda s=self: s.cmd.do("_ cmd.show_help('mouse')"))      

#      self.menuBar.addmenuitem('Help', 'command', 'Help on the Keyboard',
#                               label='Keyboard',
#                               command = lambda s=self: s.cmd.do("_ cmd.show_help('keyboard')"))      

#      self.menuBar.addmenuitem('Help', 'command', 'Help on Molecular Editing',
#                               label='Molecular Editing',
#                               command = lambda s=self: s.cmd.do("_ cmd.show_help('editing')"))      

#      self.menuBar.addmenuitem('Help', 'command', 'Help on Molecular Editing',
#                               label='Molecular Editing Keys',
#                               command = lambda s=self: s.cmd.do("_ cmd.show_help('edit_keys')"))      

#      self.menuBar.addmenuitem('Help', 'command', 'Help on Stereo',
#                               label='Stereo',
#                               command = lambda s=self: s.cmd.do("_ cmd.show_help('stereo')"))      

#      self.menuBar.addmenuitem('Help', 'separator', '')
        

#      self.menuBar.addmenuitem('Help', 'command', 'Help on the API',
#                               label='API',
#                               command = lambda s=self: s.cmd.do("_ cmd.show_help('api')"))      

        self.toggleBalloonVar = IntVar()
        self.toggleBalloonVar.set(0)
        self.setting = Setting(self.app)

#      self.menuBar.addmenuitem('Help', 'separator', '')
        
#      self.menuBar.addmenuitem('Help', 'checkbutton',
#                         'Toggle balloon help',
#                         label='Balloon help',
#                        variable = self.toggleBalloonVar,
#                        command=self.toggleBalloon)

        self.menuBar.addmenu('File', 'File Input',tearoff=TRUE)

        self.menuBar.addmenuitem('File', 'command', 'Open structure file.',
                                label='Open...',
                                command=self.file_open)

        if _wincheck():
            self.menuBar.addmenuitem('File', 'command', 'Autoload MTZ file.',
                                     label='Open MTZ with Defaults...',
                                     command=self.file_autoload_mtz)

        self.menuBar.addmenuitem('File', 'command', 'Save session.',
                                label='Save Session',
                                command=self.session_save)

        self.menuBar.addmenuitem('File', 'command', 'Save session.',
                                label='Save Session As...',
                                command=self.session_save_as)

        self.menuBar.addmenuitem('File', 'command', 'Save structure file.',
                                label='Save Molecule...',
                                command=self.file_save)

#      self.menuBar.addmenuitem('File', 'command', 'Open sequential files.',
#                        label='Open Sequence...',
#                        command=self.file_open)

        self.menuBar.addcascademenu('File', 'SaveImageAs', 'Save Image As',
                                             label='Save Image As',tearoff=FALSE)

        self.menuBar.addmenuitem('SaveImageAs', 'command', 'Save current image as PNG Image.',
                                label='PNG...',
                                command=self.file_save_png)

        self.menuBar.addmenuitem('SaveImageAs', 'separator', '')
        
        self.menuBar.addmenuitem('SaveImageAs', 'command', 'Save current image as VRML.',
                                label='VRML 2...',
                                command=self.file_save_wrl)
        
        self.menuBar.addmenuitem('SaveImageAs', 'command', 'Save current image as PovRay input.',
                                label='POV-Ray...',
                                command=self.file_save_pov)

        self.menuBar.addcascademenu('File', 'SaveMovieAs', 'Save Movie As',
                                    label='Save Movie As',tearoff=FALSE)

        self.menuBar.addmenuitem('SaveMovieAs', 'command', 'Save all frames as an MPEG movie.',
                                label='MPEG...',
                                command=self.file_save_mpeg)

        self.menuBar.addmenuitem('SaveMovieAs', 'separator', '')
        
        self.menuBar.addmenuitem('SaveMovieAs', 'command', 'Save all frames as images.',
                                label='PNG Images...',
                                command=self.file_save_mpng)

        self.menuBar.addmenuitem('File', 'separator', '')
        
        self.menuBar.addmenuitem('File', 'command', 'Open log file.',
                                label='Log...',
                                command=self.log_open)

        self.menuBar.addmenuitem('File', 'command', 'Resume log file.',
                                label='Resume...',
                                command=self.log_resume)

        self.menuBar.addmenuitem('File', 'command', 'Append log file.',
                                label='Append...',
                                command=self.log_append)

        self.menuBar.addmenuitem('File', 'command', 'Close log file.',
                                label='Close Log',
                                command=self.cmd.log_close)

        self.menuBar.addmenuitem('File', 'command', 'Run program or script.',
                                label='Run...',
                                command=self.file_run)
        
        self.menuBar.addmenuitem('File', 'separator', '')

        self.menuBar.addmenuitem('File', 'command', 'Edit pymolrc',
                                label='Edit pymolrc',
                                command=self.edit_pymolrc)

        self.menuBar.addmenuitem('File', 'separator', '')

        self.menuBar.addmenuitem('File', 'command', 'Quit PyMOL',
                                label='Quit',
                                command=self.confirm_quit)

        self.menuBar.addmenuitem('File', 'command', 'Reinitialize PyMOL',
                                label='Reinitialize',
                                command=self.cmd.reinitialize)

        self.menuBar.addmenuitem('File', 'separator', '')

        self.menuBar.addcascademenu('File', 'Skin', 'Skin',
                                             label='Skin')

        self.app.addSkinMenuItems(self.menuBar,'Skin')

#      self.menuBar.addmenuitem('File', 'separator', '')
        
#      self.menuBar.addmenuitem('File', 'checkbutton',
#                         'Log Conformations.',
#                         label='Log Conformations',
#                        variable = self.setting.log_conformations,
#                        )

#      self.menuBar.addmenuitem('File', 'checkbutton',
#                         'Log Box Selections.',
#                         label='Log Box Selections',
#                        variable = self.setting.log_box_selections,
#                        )

        self.menuBar.addmenu('Edit', 'Edit',tearoff=TRUE)

        if sys.platform == 'win32':
            if self.app.pymol.invocation.options.incentive_product:
                self.menuBar.addmenuitem('Edit', 'command',
                                     'Copy Image',
                                     label='Copy Image to Clipboard',
                                     command = lambda s=self:s.cmd.copy_image(quiet=0))

                self.menuBar.addmenuitem('Edit', 'separator', '')
        

        self.menuBar.addmenuitem('Edit', 'command', 'Undo',
                                         label='Undo [Ctrl-Z]',
                                         command = lambda s=self: s.cmd.do("_ undo"))

        self.menuBar.addmenuitem('Edit', 'command', 'Redo',
                                         label='Redo [Ctrl-Y]',
                                         command = lambda s=self: s.cmd.do("_ redo"))

        self.menuBar.addmenuitem('Edit', 'separator', '')
        
        self.menuBar.addmenuitem('Edit', 'command',
                                 'To Copy Text: Use Ctrl-C in TclTk GUI',
                                 label='To copy text use Ctrl-C in the TclTk GUI',
                                         state='disabled',
                                command =  None)

        self.menuBar.addmenuitem('Edit', 'command',
                                 'To Paste Text, Use Ctrl-V in TclTk GUI',
                                 label='To paste text use Ctrl-V in the TckTk GUI',
                                         state='disabled',                               
                                command =  None)

        if sys.platform == 'win32':
            if self.app.pymol.invocation.options.incentive_product:
                self.menuBar.addmenuitem('Edit', 'separator', '')
        
                self.menuBar.addmenuitem('Edit', 'checkbutton',
                                 'Auto-Copy Images',
                                 label='Auto-Copy Images',
                                 variable = self.setting.auto_copy_images,
                                 )

        self.menuBar.addmenu('Build', 'Build',tearoff=TRUE)

        self.menuBar.addcascademenu('Build', 'Fragment', 'Fragment',
                                             label='Fragment',tearoff=TRUE)
        
#      self.menuBar.addmenu('Fragment', 'Fragment')

        self.menuBar.addmenuitem('Fragment', 'command', 'Acetylene',
                                         label='Acetylene [Alt-J]',
                                         command = lambda s=self: s.cmd.do(
            "_ editor.attach_fragment('pk1','acetylene',2,0)"))

        self.menuBar.addmenuitem('Fragment', 'command', 'Amide N->C',
                                         label='Amide N->C [Alt-1]',
                                         command = lambda s=self: s.cmd.do(
            "_ editor.attach_fragment('pk1','formamide',3,1)"))

        self.menuBar.addmenuitem('Fragment', 'command', 'Amide C->N',
                                         label='Amide C->N [Alt-2]',
                                         command = lambda s=self: s.cmd.do(
            "_ editor.attach_fragment('pk1','formamide',5,0)"))

        self.menuBar.addmenuitem('Fragment', 'command', 'Bromine',
                                         label='Bromine [Ctrl-Shift-B]',
                                         command = lambda s=self: s.cmd.do("_ replace Br,1,1"))

        self.menuBar.addmenuitem('Fragment', 'command', 'Carbon',
                                         label='Carbon [Ctrl-Shift-C]',
                                         command = lambda s=self: s.cmd.do("_ replace C,4,4"))

        self.menuBar.addmenuitem('Fragment', 'command', 'Carbonyl',
                                         label='Carbonyl [Alt-0]',
                                         command = lambda s=self: s.cmd.do(
            "_ editor.attach_fragment('pk1','formaldehyde',2,0)"))

        self.menuBar.addmenuitem('Fragment', 'command', 'Chlorine',
                                         label='Chlorine [Ctrl-Shift-L]',
                                         command = lambda s=self: s.cmd.do("_ replace Cl,1,1"))

        self.menuBar.addmenuitem('Fragment', 'command', 'Cyclobutyl',
                                         label='Cyclobutyl [Alt-4]',
                                         command = lambda s=self: s.cmd.do(
            "_ editor.attach_fragment('pk1','cyclobutane',4,0)"))

        self.menuBar.addmenuitem('Fragment', 'command', 'Cyclopentyl',
                                         label='Cyclopentyl [Alt-5]',
                                         command = lambda s=self: s.cmd.do(
            "_ editor.attach_fragment('pk1','cyclopentane',5,0)"))

        self.menuBar.addmenuitem('Fragment', 'command', 'Cyclopentadiene',
                                         label='Cyclopentadiene [Alt-8]',
                                         command = lambda s=self: s.cmd.do(
              "_ editor.attach_fragment('pk1','cyclopentadiene',5,0)"))

        self.menuBar.addmenuitem('Fragment', 'command', 'Cyclohexyl',
                                         label='Cyclohexyl [Alt-6]',
                                         command = lambda s=self: s.cmd.do(
            "_ editor.attach_fragment('pk1','cyclohexane',7,0)"))

        self.menuBar.addmenuitem('Fragment', 'command', 'Cycloheptyl',
                                         label='Cycloheptyl [Alt-7]',
                                         command = lambda s=self: s.cmd.do(
            "_ editor.attach_fragment('pk1','cycloheptane',8,0)"))

        self.menuBar.addmenuitem('Fragment', 'command', 'Fluorine',
                                         label='Fluorine [Ctrl-Shift-F]',
                                         command = lambda s=self: s.cmd.do("_ replace F,1,1"))

        self.menuBar.addmenuitem('Fragment', 'command', 'Iodine',
                                         label='Iodine [Ctrl-Shift-I]',
                                         command = lambda s=self: s.cmd.do("_ replace I,1,1"))

        self.menuBar.addmenuitem('Fragment', 'command', 'Methane',
                                         label='Methane [Ctrl-Shift-M]',
                                         command = lambda s=self: s.cmd.do(
            "_ editor.attach_fragment('pk1','methane',1,0)"))

        self.menuBar.addmenuitem('Fragment', 'command', 'Nitrogen',
                                         label='Nitrogen [Ctrl-Shift-N]',
                                         command = lambda s=self: s.cmd.do("_ replace N,4,3"))

        self.menuBar.addmenuitem('Fragment', 'command', 'Oxygen',
                                         label='Oxygen [Ctrl-Shift-O]',
                                         command = lambda s=self: s.cmd.do("_ replace O,4,2"))

        self.menuBar.addmenuitem('Fragment', 'command', 'Phenyl',
                                         label='Phenyl [Alt-9]',
                                         command = lambda s=self: s.cmd.do(
            "_ editor.attach_fragment('pk1','benzene',6,0)"))

        self.menuBar.addmenuitem('Fragment', 'command', 'Sulfer',
                                         label='Sulfer [Ctrl-Shift-S]',
                                         command = lambda s=self: s.cmd.do("_ replace S,2,2"))

        self.menuBar.addmenuitem('Fragment', 'command', 'Sulfonyl',
                                         label='Sulfonyl [Alt-3]',
                                         command = lambda s=self: s.cmd.do(
            "_ editor.attach_fragment('pk1','sulfone',3,1)"))

        self.menuBar.addmenuitem('Fragment', 'command', 'Phosphorus',
                                         label='Phosphorus [Ctrl-Shift-P]',
                                         command = lambda s=self: s.cmd.do("_ replace P,4,3"))

#      self.menuBar.addmenu('Residue', 'Residue')

        self.menuBar.addcascademenu('Build', 'Residue', 'Residue',
                                             label='Residue',tearoff=TRUE)

 
        self.menuBar.addmenuitem('Residue', 'command', 'Acetyl',
                                         label='Acetyl [Alt-B]',
                                         command = lambda s=self: s.cmd.do(
            "_ editor.attach_amino_acid('pk1','ace')"))

        self.menuBar.addmenuitem('Residue', 'command', 'Alanine',
                                         label='Alanine [Alt-A]',
                                         command = lambda s=self: s.cmd.do(
            "_ editor.attach_amino_acid('pk1','ala')"))

        self.menuBar.addmenuitem('Residue', 'command', 'Amine',
                                         label='Amine',
                                         command = lambda s=self: s.cmd.do(
            "_ editor.attach_amino_acid('pk1','nhh')"))

        self.menuBar.addmenuitem('Residue', 'command', 'Aspartate',
                                         label='Aspartate [Alt-D]',
                                         command = lambda s=self: s.cmd.do(
            "_ editor.attach_amino_acid('pk1','asp')"))

        self.menuBar.addmenuitem('Residue', 'command', 'Asparagine',
                                         label='Asparagine [Alt-N]',
                                         command = lambda s=self: s.cmd.do(
            "_ editor.attach_amino_acid('pk1','asn')"))

        self.menuBar.addmenuitem('Residue', 'command', 'Arginine',
                                         label='Arginine [Alt-R]',
                                         command = lambda s=self: s.cmd.do(
            "_ editor.attach_amino_acid('pk1','arg')"))

        self.menuBar.addmenuitem('Residue', 'command', 'Cysteine',
                                         label='Cysteine [Alt-C]',
                                         command = lambda s=self: s.cmd.do(
            "_ editor.attach_amino_acid('pk1','cys')"))

        self.menuBar.addmenuitem('Residue', 'command', 'Glutamate',
                                         label='Glutamate [Alt-E]',
                                         command = lambda s=self: s.cmd.do(
            "_ editor.attach_amino_acid('pk1','glu')"))

        self.menuBar.addmenuitem('Residue', 'command', 'Glutamine',
                                         label='Glutamine [Alt-Q]',
                                         command = lambda s=self: s.cmd.do(
            "_ editor.attach_amino_acid('pk1','gln')"))

        self.menuBar.addmenuitem('Residue', 'command', 'Glycine',
                                         label='Glycine [Alt-G]',
                                         command = lambda s=self: s.cmd.do(
            "_ editor.attach_amino_acid('pk1','gly')"))

        self.menuBar.addmenuitem('Residue', 'command', 'Histidine',
                                         label='Histidine [Alt-H]',
                                         command = lambda s=self: s.cmd.do(
            "_ editor.attach_amino_acid('pk1','his')"))

        self.menuBar.addmenuitem('Residue', 'command', 'Isoleucine',
                                         label='Isoleucine [Alt-I]',
                                         command = lambda s=self: s.cmd.do(
            "_ editor.attach_amino_acid('pk1','ile')"))

        self.menuBar.addmenuitem('Residue', 'command', 'Leucine',
                                         label='Leucine [Alt-L]',
                                         command = lambda s=self: s.cmd.do(
            "_ editor.attach_amino_acid('pk1','leu')"))

        self.menuBar.addmenuitem('Residue', 'command', 'Lysine',
                                         label='Lysine [Alt-K]',
                                         command = lambda s=self: s.cmd.do(
            "_ editor.attach_amino_acid('pk1','lys')"))

        self.menuBar.addmenuitem('Residue', 'command', 'Methionine',
                                         label='Methionine [Alt-M]',
                                         command = lambda s=self: s.cmd.do(
            "_ editor.attach_amino_acid('pk1','met')"))

        self.menuBar.addmenuitem('Residue', 'command', 'N-Methyl',
                                         label='N-Methyl [Alt-Z]',
                                         command = lambda s=self: s.cmd.do(
            "_ editor.attach_amino_acid('pk1','nme')"))

        self.menuBar.addmenuitem('Residue', 'command', 'Phenylalanine',
                                         label='Phenylalanine [Alt-F]',
                                         command = lambda s=self: s.cmd.do(
            "_ editor.attach_amino_acid('pk1','phe')"))

        self.menuBar.addmenuitem('Residue', 'command', 'Proline',
                                         label='Proline [Alt-P]',
                                         command = lambda s=self: s.cmd.do(
            "_ editor.attach_amino_acid('pk1','pro')"))

        self.menuBar.addmenuitem('Residue', 'command', 'Serine',
                                         label='Serine [Alt-S]',
                                         command = lambda s=self: s.cmd.do(
            "_ editor.attach_amino_acid('pk1','ser')"))

        self.menuBar.addmenuitem('Residue', 'command', 'Threonine',
                                         label='Threonine [Alt-T]',
                                         command = lambda s=self: s.cmd.do(
            "_ editor.attach_amino_acid('pk1','thr')"))

        self.menuBar.addmenuitem('Residue', 'command', 'Tryptophan',
                                         label='Tryptophan [Alt-W]',
                                         command = lambda s=self: s.cmd.do(
            "_ editor.attach_amino_acid('pk1','trp')"))

        self.menuBar.addmenuitem('Residue', 'command', 'Tyrosine',
                                         label='Tyrosine [Alt-Y]',
                                         command = lambda s=self: s.cmd.do(
            "_ editor.attach_amino_acid('pk1','tyr')"))

        self.menuBar.addmenuitem('Residue', 'command', 'Valine',
                                         label='Valine [Alt-V]',
                                         command = lambda s=self: s.cmd.do(
            "_ editor.attach_amino_acid('pk1','val')"))


        self.menuBar.addmenuitem('Residue', 'separator', '')
        self.menuBar.addmenuitem('Residue', 'command', 'Helix',
                                         label='Helix',
                                         command = lambda s=self: s.cmd.do("_ set secondary_structure,1"))

        self.menuBar.addmenuitem('Residue', 'command', 'Antiparallel Beta Sheet',
                                         label='Antiparallel Beta Sheet',
                                         command = lambda s=self: s.cmd.do("_ set secondary_structure,2"))

        self.menuBar.addmenuitem('Residue', 'command', 'Parallel Beta Sheet',
                                         label='Parallel Beta Sheet',
                                         command = lambda s=self: s.cmd.do("_ set secondary_structure,3"))

        self.menuBar.addmenuitem('Build', 'separator', '')

        self.menuBar.addcascademenu('Build', 'Sculpting', 'Sculpting',
                                             label='Sculpting',tearoff=TRUE)

        self.menuBar.addmenuitem('Sculpting', 'checkbutton',
                                 'Auto-Sculpt.',
                                 label='Auto-Sculpting',
                                variable = self.setting.auto_sculpt,
                                )

        self.menuBar.addmenuitem('Sculpting', 'checkbutton',
                                 'Sculpting.',
                                 label='Sculpting',
                                variable = self.setting.sculpting,
                                )

        self.menuBar.addmenuitem('Sculpting', 'separator', '')
        

        self.menuBar.addmenuitem('Sculpting', 'command', 'Activate',
                                         label='Activate',
                                         command = lambda s=self: s.cmd.do("_ sculpt_activate all"))

        self.menuBar.addmenuitem('Sculpting', 'command', 'Deactivate',
                                         label='Deactivate',
                                         command = lambda s=self: s.cmd.do("_ sculpt_deactivate all"))

        self.menuBar.addmenuitem('Sculpting', 'command', 'Clear Memory',
                                         label='Clear Memory',
                                         command = lambda s=self: s.cmd.do("_ sculpt_purge"))

        self.menuBar.addmenuitem('Sculpting', 'separator', '')

        self.menuBar.addmenuitem('Sculpting', 'command', '1 Cycle/Update',
                                         label='1 Cycle per Update',
                                         command = lambda s=self: s.cmd.do("_ set sculpting_cycles=1"))

        self.menuBar.addmenuitem('Sculpting', 'command', '3 Cycles/Update',
                                         label='3 Cycles per Update',
                                         command = lambda s=self: s.cmd.do("_ set sculpting_cycles=3"))

        self.menuBar.addmenuitem('Sculpting', 'command', '10 Cycles/Update',
                                         label='10 Cycles per Update',
                                         command = lambda s=self: s.cmd.do("_ set sculpting_cycles=10"))

        self.menuBar.addmenuitem('Sculpting', 'command', '33 Cycles/Update',
                                         label='33 Cycles per Update',
                                         command = lambda s=self: s.cmd.do("_ set sculpting_cycles=33"))

        self.menuBar.addmenuitem('Sculpting', 'command', '100 Cycles/Update',
                                         label='100 Cycles per Update',
                                         command = lambda s=self: s.cmd.do("_ set sculpting_cycles=100"))

        self.menuBar.addmenuitem('Sculpting', 'command', '333 Cycles/Update',
                                         label='333 Cycles per Update',
                                         command = lambda s=self: s.cmd.do("_ set sculpting_cycles=333"))

        self.menuBar.addmenuitem('Sculpting', 'command', '1000 Cycles/Update',
                                         label='1000 Cycles per Update',
                                         command = lambda s=self: s.cmd.do("_ set sculpting_cycles=1000"))

        self.menuBar.addmenuitem('Sculpting', 'separator', '')

#define cSculptBond  0x01
#define cSculptAngl  0x02
#define cSculptPyra  0x04
#define cSculptPlan  0x08
#define cSculptLine  0x10
#define cSculptVDW   0x20
#define cSculptVDW14 0x40
#define cSculptTors  0x80

        self.menuBar.addmenuitem('Sculpting', 'command', 'Bonds Only',
                                         label='Bonds Only',
                                         command = lambda s=self: s.cmd.do("_ set sculpt_field_mask=%d"%(
            0x01)))

        self.menuBar.addmenuitem('Sculpting', 'command', 'Bonds & Angles Only',
                                         label='Bonds & Angles Only',
                                         command = lambda s=self: s.cmd.do("_ set sculpt_field_mask=%d"%(
            0x01+0x02)))
        
        self.menuBar.addmenuitem('Sculpting', 'command', 'Local Geometry Only',
                                         label='Local Geometry Only',
                                         command = lambda s=self: s.cmd.do("_ set sculpt_field_mask=%d"%(
            0x01+0x02+0x04+0x08+0x10)))

        self.menuBar.addmenuitem('Sculpting', 'command', 'All Except VDW',
                                         label='All Except VDW',
                                         command = lambda s=self: s.cmd.do("_ set sculpt_field_mask=%d"%(
            0x01+0x02+0x04+0x08+0x10+0x80)))

        self.menuBar.addmenuitem('Sculpting', 'command', 'All Except 1-4 VDW & Torsions',
                                         label='All Except 1-4 VDW & Torsions',
                                         command = lambda s=self: s.cmd.do("_ set sculpt_field_mask=%d"%(
            0x01+0x02+0x04+0x08+0x10+0x20)))

        self.menuBar.addmenuitem('Sculpting', 'command', 'All Terms',
                                         label='All Terms',
                                         command = lambda s=self: s.cmd.do("_ set sculpt_field_mask=%d"%(
            0xFF)))


        self.menuBar.addmenuitem('Build', 'separator', '')
        
        self.menuBar.addmenuitem('Build', 'command', 'Cycle Bond Valence',
                                         label='Cycle Bond Valence [Ctrl-Shift-W]',
                                         command = lambda s=self: s.cmd.do("_ cycle_valence"))

        self.menuBar.addmenuitem('Build', 'command', 'Fill Hydrogens',
                                         label='Fill Hydrogens on (pk1) [Ctrl-Shift-R]',
                                         command = lambda s=self: s.cmd.do("_ h_fill"))

        self.menuBar.addmenuitem('Build', 'command', 'Invert',
                                         label='Invert (pk2)-(pk1)-(pk3) [Ctrl-Shift-E]',
                                         command = lambda s=self: s.cmd.do("_ invert"))

        self.menuBar.addmenuitem('Build', 'command', 'Form Bond',
                                         label='Create Bond (pk1)-(pk2) [Ctrl-Shift-T]',
                                         command = lambda s=self: s.cmd.do("_ bond"))


        self.menuBar.addmenuitem('Build', 'separator', '')

        
        self.menuBar.addmenuitem('Build', 'command', 'Remove (pk1)',
                                         label='Remove (pk1) [Ctrl-Shift-D]',
                                         command = lambda s=self: s.cmd.do("_ remove pk1"))

        self.menuBar.addmenuitem('Build', 'separator', '')
        
        self.menuBar.addmenuitem('Build', 'command', 'Make Positive',
                                 label='Make (pk1) Positive [Ctrl-Shift-K]',
                                 command = lambda s=self: s.cmd.do("_ alter pk1,formal_charge=1.0"))
        
        self.menuBar.addmenuitem('Build', 'command', 'Make Negative',
                                 label='Make (pk1) Negative [Ctrl-Shift-J]',
                                 command = lambda s=self: s.cmd.do("_ alter pk1,formal_charge=-1.0"))
        
        self.menuBar.addmenuitem('Build', 'command', 'Make Neutral',
                                 label='Make (pk1) Neutral [Ctrl-Shift-U]',
                                 command = lambda s=self: s.cmd.do("_ alter pk1,formal_charge=-0.0"))

        self.menuBar.addmenu('Movie', 'Movie Control',tearoff=TRUE)
        
        self.menuBar.addcascademenu('Movie', 'Append', 'Append',
                                    label='Append')

        self.menuBar.addmenuitem('Append', 'command', '0.25 second',label='0.25 second',
                                 command = lambda s=self: s.cmd.do("_ movie.add_blank(0.25)"))

        self.menuBar.addmenuitem('Append', 'command', '0.5 second',label='0.5 second',
                                 command = lambda s=self: s.cmd.do("_ movie.add_blank(0.5)"))

        self.menuBar.addmenuitem('Append', 'command', '1 second',label='1 second',
                                 command = lambda s=self: s.cmd.do("_ movie.add_blank(1.0)"))

        self.menuBar.addmenuitem('Append', 'command', '2 seconds',label='2 seconds',
                                 command = lambda s=self: s.cmd.do("_ movie.add_blank(2.0)"))

        self.menuBar.addmenuitem('Append', 'command', '3 seconds',label='3 seconds',
                                 command = lambda s=self: s.cmd.do("_ movie.add_blank(3.0)"))

        self.menuBar.addmenuitem('Append', 'command', '4 seconds',label='4 seconds',
                                 command = lambda s=self: s.cmd.do("_ movie.add_blank(4.0)"))

        self.menuBar.addmenuitem('Append', 'command', '6 seconds',label='6 seconds',
                                 command = lambda s=self: s.cmd.do("_ movie.add_blank(6.0)"))

        self.menuBar.addmenuitem('Append', 'command', '8 seconds',label='8 seconds',
                                 command = lambda s=self: s.cmd.do("_ movie.add_blank(8.0)"))

        self.menuBar.addmenuitem('Append', 'command', '12 seconds',label='12 seconds',
                                 command = lambda s=self: s.cmd.do("_ movie.add_blank(12.0)"))

        self.menuBar.addmenuitem('Append', 'command', '18 seconds',label='18 seconds',
                                 command = lambda s=self: s.cmd.do("_ movie.add_blank(18.0)"))

        self.menuBar.addmenuitem('Append', 'command', '24 seconds',label='24 seconds',
                                 command = lambda s=self: s.cmd.do("_ movie.add_blank(24.0)"))

        self.menuBar.addmenuitem('Append', 'command', '30 seconds',label='30 seconds',
                                 command = lambda s=self: s.cmd.do("_ movie.add_blank(30.0)"))

        self.menuBar.addmenuitem('Append', 'command', '48 seconds',label='48 seconds',
                                 command = lambda s=self: s.cmd.do("_ movie.add_blank(48.0)"))

        self.menuBar.addmenuitem('Append', 'command', '60 seconds',label='60 seconds',
                                 command = lambda s=self: s.cmd.do("_ movie.add_blank(60.0)"))

        self.menuBar.addcascademenu('Movie', 'Program', 'Program',
                                    label='Program')

        self.menuBar.addmenuitem('Movie', 'command', 'Update Last Program',label='Update Last Program',
                                 command = lambda s=self: s.mvprg())

        self.menuBar.addmenuitem('Movie', 'command', 'Remove Last Program',label='Remove Last Program',
                                 command = lambda s=self: s.mvprg(-1))

        self.menuBar.addcascademenu('Program', 'Camera', 'Camera Loop',
                                    label='Camera Loop')

        self.menuBar.addcascademenu('Camera', 'Nutate', 'Nutate',
                                    label='Nutate')

        self.menuBar.addmenuitem('Camera', 'separator', '')

        self.menuBar.addmenuitem('Nutate', 'command', '15 deg. over 4 sec.',label='15 deg. over 4 sec.',
                                 command = lambda s=self: s.mvprg("_ movie.add_nutate(4,15,start=%d)"))

        self.menuBar.addmenuitem('Nutate', 'command', '15 deg. over 8 sec.',label='15 deg. over 8 sec.',
                                 command = lambda s=self: s.mvprg("_ movie.add_nutate(8,15,start=%d)"))

        self.menuBar.addmenuitem('Nutate', 'command', '15 deg. over 12 sec.',label='15 deg. over 12 sec.',
                                 command = lambda s=self: s.mvprg("_ movie.add_nutate(12,15,start=%d)"))

        self.menuBar.addmenuitem('Nutate', 'separator', '')

        self.menuBar.addmenuitem('Nutate', 'command', '30 deg. over 4 sec.',label='30 deg. over 4 sec.',
                                 command = lambda s=self: s.mvprg("_ movie.add_nutate(4,30,start=%d)"))

        self.menuBar.addmenuitem('Nutate', 'command', '30 deg. over 8 sec.',label='30 deg. over 8 sec.',
                                 command = lambda s=self: s.mvprg("_ movie.add_nutate(8,30,start=%d)"))

        self.menuBar.addmenuitem('Nutate', 'command', '30 deg. over 12 sec.',label='30 deg. over 12 sec.',
                                 command = lambda s=self: s.mvprg("_ movie.add_nutate(12,30,start=%d)"))

        self.menuBar.addmenuitem('Nutate', 'command', '30 deg. over 16 sec.',label='30 deg. over 16 sec.',
                                 command = lambda s=self: s.mvprg("_ movie.add_nutate(16,30,start=%d)"))

        self.menuBar.addmenuitem('Nutate', 'separator', '')

        self.menuBar.addmenuitem('Nutate', 'command', '60 deg. over 8 sec.',label='60 deg. over 8 sec.',
                                 command = lambda s=self: s.mvprg("_ movie.add_nutate(8,60,start=%d)"))

        self.menuBar.addmenuitem('Nutate', 'command', '60 deg. over 16 sec.',label='60 deg. over 16 sec.',
                                 command = lambda s=self: s.mvprg("_ movie.add_nutate(16,60,start=%d)"))

        self.menuBar.addmenuitem('Nutate', 'command', '60 deg. over 24 sec.',label='60 deg. over 24 sec.',
                                 command = lambda s=self: s.mvprg("_ movie.add_nutate(24,60,start=%d)"))

        self.menuBar.addmenuitem('Nutate', 'command', '60 deg. over 32 sec.',label='60 deg. over 32 sec.',
                                 command = lambda s=self: s.mvprg("_ movie.add_nutate(32,60,start=%d)"))

        self.menuBar.addcascademenu('Camera', 'X-Rock', 'X-Rock',
                                    label='X-Rock')
        
        self.menuBar.addmenuitem('X-Rock', 'command', '30 deg. over 2 sec.',label='30 deg. over 2 sec.',
                                 command = lambda s=self: s.mvprg("_ movie.add_rock(2,30,axis='x',start=%d)"))

        self.menuBar.addmenuitem('X-Rock', 'command', '30 deg. over 4 sec.',label='30 deg. over 4 sec.',
                                 command = lambda s=self: s.mvprg("_ movie.add_rock(4,30,axis='x',start=%d)"))

        self.menuBar.addmenuitem('X-Rock', 'command', '30 deg. over 8 sec.',label='30 deg. over 8 sec.',
                                 command = lambda s=self: s.mvprg("_ movie.add_rock(8,30,axis='x',start=%d)"))
        
        self.menuBar.addmenuitem('X-Rock', 'separator', '')
        
        self.menuBar.addmenuitem('X-Rock', 'command', '60 deg. over 4 sec.',label='60 deg. over 4 sec.',
                                 command = lambda s=self: s.mvprg("_ movie.add_rock(4,60,axis='x',start=%d)"))

        self.menuBar.addmenuitem('X-Rock', 'command', '60 deg. over 8 sec.',label='60 deg. over 8 sec.',
                                 command = lambda s=self: s.mvprg("_ movie.add_rock(8,60,axis='x',start=%d)"))

        self.menuBar.addmenuitem('X-Rock', 'command', '60 deg. over 16 sec.',label='60 deg. over 16 sec.',
                                 command = lambda s=self: s.mvprg("_ movie.add_rock(16,60,axis='x',start=%d)"))

        self.menuBar.addmenuitem('X-Rock', 'separator', '')
        
        self.menuBar.addmenuitem('X-Rock', 'command', '90 deg. over 6 sec.',label='90 deg. over 6 sec.',
                                 command = lambda s=self: s.mvprg("_ movie.add_rock(6,90,axis='x',start=%d)"))

        self.menuBar.addmenuitem('X-Rock', 'command', '90 deg. over 12 sec.',label='90 deg. over 12 sec.',
                                 command = lambda s=self: s.mvprg("_ movie.add_rock(12,90,axis='x',start=%d)"))

        self.menuBar.addmenuitem('X-Rock', 'command', '90 deg. over 24 sec.',label='90 deg. over 24 sec.',
                                 command = lambda s=self: s.mvprg("_ movie.add_rock(24,90,axis='x',start=%d)"))

        self.menuBar.addmenuitem('X-Rock', 'separator', '')
        
        self.menuBar.addmenuitem('X-Rock', 'command', '120 deg. over 8 sec.',label='120 deg. over 8 sec.',
                                 command = lambda s=self: s.mvprg("_ movie.add_rock(8,120,axis='x',start=%d)"))

        self.menuBar.addmenuitem('X-Rock', 'command', '120 deg. over 16 sec.',label='120 deg. over 16 sec.',
                                 command = lambda s=self: s.mvprg("_ movie.add_rock(16,120,axis='x',start=%d)"))

        self.menuBar.addmenuitem('X-Rock', 'command', '120 deg. over 32 sec.',label='120 deg. over 32 sec.',
                                 command = lambda s=self: s.mvprg("_ movie.add_rock(32,120,axis='x',start=%d)"))

        self.menuBar.addmenuitem('X-Rock', 'separator', '')
        
        self.menuBar.addmenuitem('X-Rock', 'command', '180 deg. over 12 sec.',label='180 deg. over 12 sec.',
                                 command = lambda s=self: s.mvprg("_ movie.add_rock(12,179.99,axis='x',start=%d)"))

        self.menuBar.addmenuitem('X-Rock', 'command', '180 deg. over 24 sec.',label='180 deg. over 24 sec.',
                                 command = lambda s=self: s.mvprg("_ movie.add_rock(24,179.99,axis='x',start=%d)"))

        self.menuBar.addmenuitem('X-Rock', 'command', '180 deg. over 48 sec.',label='180 deg. over 48 sec.',
                                 command = lambda s=self: s.mvprg("_ movie.add_rock(48,179.99,axis='x',start=%d)"))

        self.menuBar.addcascademenu('Camera', 'X-Roll', 'X-Roll',
                                    label='X-Roll')

        self.menuBar.addmenuitem('X-Roll', 'command', '4 seconds',label='4 seconds',
                                 command = lambda s=self: s.mvprg("_ movie.add_roll(4.0,axis='x',start=%d)"))

        self.menuBar.addmenuitem('X-Roll', 'command', '8 seconds',label='8 seconds',
                                 command = lambda s=self: s.mvprg("_ movie.add_roll(8.0,axis='x',start=%d)"))
        
        self.menuBar.addmenuitem('X-Roll', 'command', '16 seconds',label='16 seconds',
                                 command = lambda s=self: s.mvprg("_ movie.add_roll(16.0,axis='x',start=%d)"))
        
        self.menuBar.addmenuitem('X-Roll', 'command', '32 seconds',label='32 seconds',
                                 command = lambda s=self: s.mvprg("_ movie.add_roll(32.0,axis='x',start=%d)"))

        self.menuBar.addmenuitem('Camera', 'separator', '')

        self.menuBar.addcascademenu('Camera', 'Y-Rock', 'Y-Rock',
                                    label='Y-Rock')
        
        self.menuBar.addmenuitem('Y-Rock', 'command', '30 deg. over 2 sec.',label='30 deg. over 2 sec.',
                                 command = lambda s=self: s.mvprg("_ movie.add_rock(2,30,axis='y',start=%d)"))

        self.menuBar.addmenuitem('Y-Rock', 'command', '30 deg. over 4 sec.',label='30 deg. over 4 sec.',
                                 command = lambda s=self: s.mvprg("_ movie.add_rock(4,30,axis='y',start=%d)"))

        self.menuBar.addmenuitem('Y-Rock', 'command', '30 deg. over 8 sec.',label='30 deg. over 8 sec.',
                                 command = lambda s=self: s.mvprg("_ movie.add_rock(8,30,axis='y',start=%d)"))
        
        self.menuBar.addmenuitem('Y-Rock', 'separator', '')
        
        self.menuBar.addmenuitem('Y-Rock', 'command', '60 deg. over 4 sec.',label='60 deg. over 4 sec.',
                                 command = lambda s=self: s.mvprg("_ movie.add_rock(4,60,axis='y',start=%d)"))

        self.menuBar.addmenuitem('Y-Rock', 'command', '60 deg. over 8 sec.',label='60 deg. over 8 sec.',
                                 command = lambda s=self: s.mvprg("_ movie.add_rock(8,60,axis='y',start=%d)"))

        self.menuBar.addmenuitem('Y-Rock', 'command', '60 deg. over 16 sec.',label='60 deg. over 16 sec.',
                                 command = lambda s=self: s.mvprg("_ movie.add_rock(16,60,axis='y',start=%d)"))

        self.menuBar.addmenuitem('Y-Rock', 'separator', '')
        
        self.menuBar.addmenuitem('Y-Rock', 'command', '90 deg. over 6 sec.',label='90 deg. over 6 sec.',
                                 command = lambda s=self: s.mvprg("_ movie.add_rock(6,90,axis='y',start=%d)"))

        self.menuBar.addmenuitem('Y-Rock', 'command', '90 deg. over 12 sec.',label='90 deg. over 12 sec.',
                                 command = lambda s=self: s.mvprg("_ movie.add_rock(12,90,axis='y',start=%d)"))

        self.menuBar.addmenuitem('Y-Rock', 'command', '90 deg. over 24 sec.',label='90 deg. over 24 sec.',
                                 command = lambda s=self: s.mvprg("_ movie.add_rock(24,90,axis='y',start=%d)"))

        self.menuBar.addmenuitem('Y-Rock', 'separator', '')
        
        self.menuBar.addmenuitem('Y-Rock', 'command', '120 deg. over 8 sec.',label='120 deg. over 8 sec.',
                                 command = lambda s=self: s.mvprg("_ movie.add_rock(8,120,axis='y',start=%d)"))

        self.menuBar.addmenuitem('Y-Rock', 'command', '120 deg. over 16 sec.',label='120 deg. over 16 sec.',
                                 command = lambda s=self: s.mvprg("_ movie.add_rock(16,120,axis='y',start=%d)"))

        self.menuBar.addmenuitem('Y-Rock', 'command', '120 deg. over 32 sec.',label='120 deg. over 32 sec.',
                                 command = lambda s=self: s.mvprg("_ movie.add_rock(32,120,axis='y',start=%d)"))

        self.menuBar.addmenuitem('Y-Rock', 'separator', '')
        
        self.menuBar.addmenuitem('Y-Rock', 'command', '180 deg. over 12 sec.',label='180 deg. over 12 sec.',
                                 command = lambda s=self: s.mvprg("_ movie.add_rock(12,179.99,axis='y',start=%d)"))

        self.menuBar.addmenuitem('Y-Rock', 'command', '180 deg. over 24 sec.',label='180 deg. over 24 sec.',
                                 command = lambda s=self: s.mvprg("_ movie.add_rock(24,179.99,axis='y',start=%d)"))

        self.menuBar.addmenuitem('Y-Rock', 'command', '180 deg. over 48 sec.',label='180 deg. over 48 sec.',
                                 command = lambda s=self: s.mvprg("_ movie.add_rock(48,179.99,axis='y',start=%d)"))

        self.menuBar.addcascademenu('Camera', 'Y-Roll', 'Y-Roll',
                                    label='Y-Roll')

        self.menuBar.addmenuitem('Y-Roll', 'command', '4 seconds',label='4 seconds',
                                 command = lambda s=self: s.mvprg("_ movie.add_roll(4.0,axis='y',start=%d)"))

        self.menuBar.addmenuitem('Y-Roll', 'command', '8 seconds',label='8 seconds',
                                 command = lambda s=self: s.mvprg("_ movie.add_roll(8.0,axis='y',start=%d)"))
        
        self.menuBar.addmenuitem('Y-Roll', 'command', '16 seconds',label='16 seconds',
                                 command = lambda s=self: s.mvprg("_ movie.add_roll(16.0,axis='y',start=%d)"))
        
        self.menuBar.addmenuitem('Y-Roll', 'command', '32 seconds',label='32 seconds',
                                 command = lambda s=self: s.mvprg("_ movie.add_roll(32.0,axis='y',start=%d)"))

        self.menuBar.addmenuitem('Program', 'separator', '')
        
        self.menuBar.addcascademenu('Program', 'Scene Loop', 'Scene Loop',
                                    label='Scene Loop')

        for label, rock in [('Nutate', 4), ('X-Rock', 2), ('Y-Rock', 1)]:
            mlabel = 'SL-' + label
            self.menuBar.addcascademenu('Scene Loop', mlabel, label, label=label)

            for angle, seconds in ((30, (2,4,8)), (60, (4,8,16)), (90, (6,12,24)), (120, (8,16,32))):
                if angle != 30:
                    self.menuBar.addmenuitem(mlabel, 'separator', '')
                for sec in seconds:
                    label = '%d deg. over %d sec.' % (angle, sec)
                    self.menuBar.addmenuitem(mlabel, 'command', label, label=label,
                            command=self.mvprg_scene_loop(sec, rock, angle))

        self.menuBar.addcascademenu('Scene Loop', 'No-Motion', 'Steady',
                                    label='Steady')

        self.menuBar.addmenuitem('No-Motion', 'command', '1 second each',label='1 second each',
                                 command = lambda s=self: s.mvprg("_ movie.add_scenes(None,1.0,rock=0,start=%d)"))

        self.menuBar.addmenuitem('No-Motion', 'command', '2 seconds each',label='2 seconds each',
                                 command = lambda s=self: s.mvprg("_ movie.add_scenes(None,2.0,rock=0,start=%d)"))

        self.menuBar.addmenuitem('No-Motion', 'command', '4 seconds each',label='4 seconds each',
                                 command = lambda s=self: s.mvprg("_ movie.add_scenes(None,4.0,rock=0,start=%d)"))

        self.menuBar.addmenuitem('No-Motion', 'command', '8 seconds each',label='8 seconds each',
                                 command = lambda s=self: s.mvprg("_ movie.add_scenes(None,8.0,rock=0,start=%d)"))

        self.menuBar.addmenuitem('No-Motion', 'command', '12 seconds each',label='12 seconds each',
                                 command = lambda s=self: s.mvprg("_ movie.add_scenes(None,12.0,rock=0,start=%d)"))

        self.menuBar.addmenuitem('No-Motion', 'command', '16 seconds each',label='16 seconds each',
                                 command = lambda s=self: s.mvprg("_ movie.add_scenes(None,16.0,rock=0,start=%d)"))

        self.menuBar.addmenuitem('No-Motion', 'command', '24 seconds each',label='24 seconds each',
                                 command = lambda s=self: s.mvprg("_ movie.add_scenes(None,24.0,rock=0,start=%d)"))

        self.menuBar.addmenuitem('Program', 'separator', '')

        self.menuBar.addcascademenu('Program', 'StateLoop', 'State Loop',
                                    label='State Loop')

        self.menuBar.addcascademenu('Program', 'StateSweep', 'State Sweep',
                                    label='State Sweep')

        speed_list = [ 1, 2, 3, 4, 8, 16 ]
        pause_list = [ 0, 1, 2, 4 ]
        
        for speed in speed_list:
            submenu1_id = 'StateLoop' + '%d'%speed
            submenu2_id = 'StateSweep' + '%d'%speed

            if speed==1:
                submenu_title = "Full Speed"
            else:
                submenu_title = "1/%d Speed"%speed

            self.menuBar.addcascademenu('StateLoop', submenu1_id, label=submenu_title)
            self.menuBar.addcascademenu('StateSweep', submenu2_id, label=submenu_title)
              
            for pause in pause_list:
                if not pause:
                    item_name = "no pause"
                else:
                    item_name = "%d second pause"%pause
                
                self.menuBar.addmenuitem(submenu1_id, 'command', item_name,
                                         label=item_name,
                                         command = lambda
                                         s=self, st="_ movie.%s(%d,%d"%
                                          ("add_state_loop", speed, pause): 
                                         s.mvprg(st+",start=%d)"))

                self.menuBar.addmenuitem(submenu2_id, 'command', item_name,
                                         label=item_name,
                                         command = lambda
                                         s=self, st="_ movie.%s(%d,%d"%
                                          ("add_state_sweep", speed, pause): 
                                         s.mvprg(st+",start=%d)"))

        self.menuBar.addmenuitem('Movie', 'separator', '')

        self.menuBar.addmenuitem('Movie', 'command', 'Reset',label='Reset',
                                 command = lambda s=self: s.cmd.do("_ mset;rewind;"))

        self.menuBar.addmenuitem('Movie', 'separator', '')

        self.menuBar.addcascademenu('Movie', 'Frame Rate', 'Playback Frame Rate',
                                    label='Frame Rate')

        self.menuBar.addmenuitem('Frame Rate', 'command', 'Maximum',
                                         label='Maximum',
                                         command = lambda s=self: s.cmd.set("movie_fps","-1",log=1))

        self.menuBar.addmenuitem('Frame Rate', 'command', '30 FPS',
                                         label='30 FPS',
                                         command = lambda s=self: s.cmd.set("movie_fps","30",log=1))

        self.menuBar.addmenuitem('Frame Rate', 'command', '15 FPS',
                                         label='15 FPS',
                                         command = lambda s=self: s.cmd.set("movie_fps","15",log=1))

        self.menuBar.addmenuitem('Frame Rate', 'command', '5 FPS',
                                         label='5 FPS',
                                         command = lambda s=self: s.cmd.set("movie_fps","5",log=1))

        self.menuBar.addmenuitem('Frame Rate', 'command', '1 FPS',
                                         label='1 FPS',
                                         command = lambda s=self: s.cmd.set("movie_fps","1",log=1))

        self.menuBar.addmenuitem('Frame Rate', 'command', '0.3 FPS',
                                         label='0.3 FPS',
                                         command = lambda s=self: s.cmd.set("movie_fps","0.3",log=1))

        self.menuBar.addmenuitem('Frame Rate', 'separator', '')

        self.menuBar.addmenuitem('Frame Rate', 'checkbutton',
                                 'Show Frame Frame.',
                                 label='Show Frame Rate',
                                 variable = self.setting.show_frame_rate,
                                 )
        
        self.menuBar.addmenuitem('Frame Rate', 'command', 'Reset Meter',
                                         label='Reset Meter',
                                         command = lambda s=self: s.cmd.do("_ meter_reset"))

        self.menuBar.addmenuitem('Movie', 'separator', '')

        self.menuBar.addmenuitem('Movie', 'checkbutton',
                                 'Auto Interpolate',
                                 label='Auto Interpolate',
                                 variable = self.setting.movie_auto_interpolate,
                                 )

        self.menuBar.addmenuitem('Movie', 'checkbutton',
                                 'Show Panel',
                                 label='Show Panel',
                                 variable = self.setting.movie_panel,
                                 )

        self.menuBar.addmenuitem('Movie', 'checkbutton',
                                 'Loop Frames',
                                 label='Loop Frames',
                                 variable = self.setting.movie_loop,
                                 )


        self.menuBar.addmenuitem('Movie', 'checkbutton',
                                 'Photorealistic images.',
                                 label='Draw Frames',
                                 variable = self.setting.draw_frames,
                                 )

        self.menuBar.addmenuitem('Movie', 'checkbutton',
                                 'Photorealistic images.',
                                 label='Ray Trace Frames',
                                 variable = self.setting.ray_trace_frames,
                                 )

        self.menuBar.addmenuitem('Movie', 'checkbutton',
                                 'Save images in memory.',
                                 label='Cache Frame Images',
                                variable = self.setting.cache_frames,
                                )

        self.menuBar.addmenuitem('Movie', 'command', 'Clear Image Cache',
                                         label='Clear Image Cache',
                                         command = lambda s=self: s.cmd.mclear())

        self.menuBar.addmenuitem('Movie', 'separator', '')

        self.menuBar.addmenuitem('Movie', 'checkbutton',
                                 'Static Singletons Objects',
                                 label='Static Singletons',
                                variable = self.setting.static_singletons,
                                )

        self.menuBar.addmenuitem('Movie', 'checkbutton',
                                 'Superimpose all molecular states.',
                                 label='Show All States',
                                variable = self.setting.all_states,
                                )

        self.menuBar.addmenu('Display', 'Display Control',tearoff=TRUE)

        self.menuBar.addmenuitem('Display', 'checkbutton',
                                 'Sequence',
                                 label='Sequence',
                                variable = self.setting.seq_view,
                                )

        self.menuBar.addcascademenu('Display', 'Sequence', 'Sequence Mode',
                                         label='Sequence Mode')



        self.menuBar.addmenuitem('Sequence', 'command', 'Residue Codes',
                                         label='Residue Codes',
                                         command = lambda s=self: s.cmd.do("_ set seq_view_format,0"))

        self.menuBar.addmenuitem('Sequence', 'command', 'Residue Names',
                                         label='Residues',
                                         command = lambda s=self: s.cmd.do("_ set seq_view_format,1"))

        self.menuBar.addmenuitem('Sequence', 'command', 'Chain Identifiers',
                                         label='Chains',
                                         command = lambda s=self: s.cmd.do("_ set seq_view_format,3"))

        self.menuBar.addmenuitem('Sequence', 'command', 'Atom Names',
                                         label='Atoms',
                                         command = lambda s=self: s.cmd.do("_ set seq_view_format,2"))

        self.menuBar.addmenuitem('Sequence', 'command', 'States',
                                         label='States',
                                         command = lambda s=self: s.cmd.do("_ set seq_view_format,4"))

        self.menuBar.addmenuitem('Sequence', 'separator', '')

        self.menuBar.addmenuitem('Sequence', 'command', 'All Residue Numbers',
                                         label='All Residue Numbers',
                                         command = lambda s=self: s.cmd.do("_ set seq_view_label_mode,2"))

        self.menuBar.addmenuitem('Sequence', 'command', 'Top Sequence Only',
                                 label='Top Sequence Only',
                                 command = lambda s=self: s.cmd.do("_ set seq_view_label_mode,1"))

        self.menuBar.addmenuitem('Sequence', 'command', 'Object Names Only',
                                 label='Object Names Only',
                                 command = lambda s=self: s.cmd.do("_ set seq_view_label_mode,0"))

        self.menuBar.addmenuitem('Sequence', 'command', 'No Labels',
                                 label='No Labels',
                                 command = lambda s=self: s.cmd.do("_ set seq_view_label_mode,3"))

        self.menuBar.addmenuitem('Display', 'checkbutton',
                                 'Stereo',
                                 label='Stereo',
                                variable = self.setting.stereo,
                                )
        
#      self.menuBar.addmenuitem('Display', 'command', 'Stereo On',
#                               label='Stereo On',
#                              command = lambda s=self: s.cmd.do("_ stereo on"))

#      self.menuBar.addmenuitem('Display', 'command', 'Stereo Off',
#                               label='Stereo Off',
#                               command = lambda s=self: s.cmd.do("_ stereo off"))

        self.menuBar.addcascademenu('Display', 'Stereo', 'Stereo Mode',
                                         label='Stereo Mode')

        self.menuBar.addmenuitem('Stereo', 'command', 'Anaglyph Stereo',
                                         label='Anaglyph Stereo',
                                         command = lambda s=self: s.cmd.do("_ stereo anaglyph"))

        self.menuBar.addmenuitem('Stereo', 'command', 'Cross-Eye Stereo',
                                         label='Cross-Eye Stereo',
                                         command = lambda s=self: s.cmd.do("_ stereo crosseye"))

        self.menuBar.addmenuitem('Stereo', 'command', 'Wall-Eye Stereo',
                                         label='Wall-Eye Stereo',
                                         command = lambda s=self: s.cmd.do("_ stereo walleye"))

        self.menuBar.addmenuitem('Stereo', 'command', 'Quad-Buffered Stereo',
                                         label='Quad-Buffered Stereo',
                                         command = lambda s=self: s.cmd.do("_ stereo quadbuffer"))

        self.menuBar.addmenuitem('Stereo', 'separator', '')


        self.menuBar.addmenuitem('Stereo', 'command', 'Swap Sides',
                                         label='Swap Sides',
                                         command = lambda s=self: s.cmd.do("_ stereo swap"))

        self.menuBar.addmenuitem('Display', 'separator', '')
        
        self.menuBar.addcascademenu('Display', 'Zoom', 'Zoom',
                                             label='Zoom')

        self.menuBar.addmenuitem('Zoom', 'command', '4 Angstrom Sphere',
                                         label='4 Angstrom Sphere',
                                         command = lambda s=self: s.cmd.do("_ zoom center,4,animate=-1"))

        self.menuBar.addmenuitem('Zoom', 'command', '6 Angstrom Sphere',
                                         label='6 Angstrom Sphere',
                                         command = lambda s=self: s.cmd.do("_ zoom center,6,animate=-1"))

        self.menuBar.addmenuitem('Zoom', 'command', '8 Angstrom Sphere',
                                         label='8 Angstrom Sphere',
                                         command = lambda s=self: s.cmd.do("_ zoom center,8,animate=-1"))

        self.menuBar.addmenuitem('Zoom', 'command', '12 Angstrom Sphere',
                                         label='12 Angstrom Sphere',
                                         command = lambda s=self: s.cmd.do("_ zoom center,12,animate=-1"))

        self.menuBar.addmenuitem('Zoom', 'command', '20 Angstrom Sphere',
                                         label='20 Angstrom Sphere',
                                         command = lambda s=self: s.cmd.do("_ zoom center,20,animate=-1"))

        self.menuBar.addmenuitem('Zoom', 'command', 'All',
                                         label='All',
                                         command = lambda s=self: s.cmd.do("_ zoom all,animate=-1"))

        self.menuBar.addmenuitem('Zoom', 'command', 'Complete',
                                         label='Complete',
                                         command = lambda s=self: s.cmd.do("_ zoom all,complete=1,animate=-1"))

        self.menuBar.addcascademenu('Display', 'Clip', 'Clip',
                                             label='Clip')

        self.menuBar.addmenuitem('Clip', 'command', 'Nothing',
                                         label='Nothing',
                                         command = lambda s=self: s.cmd.do("_ clip atoms,5,all"))

        self.menuBar.addmenuitem('Clip', 'command', '8 Angstrom Slab',
                                         label='8 Angstrom Slab',
                                         command = lambda s=self: s.cmd.do("_ clip slab,8"))

        self.menuBar.addmenuitem('Clip', 'command', '12 Angstrom Slab',
                                         label='12 Angstrom Slab',
                                         command = lambda s=self: s.cmd.do("_ clip slab,10"))

        self.menuBar.addmenuitem('Clip', 'command', '16 Angstrom Slab',
                                         label='16 Angstrom Slab',
                                         command = lambda s=self: s.cmd.do("_ clip slab,15"))

        self.menuBar.addmenuitem('Clip', 'command', '20 Angstrom Slab',
                                         label='20 Angstrom Slab',
                                         command = lambda s=self: s.cmd.do("_ clip slab,20"))

        self.menuBar.addmenuitem('Clip', 'command', '30 Angstrom Slab',
                                         label='30 Angstrom Slab',
                                         command = lambda s=self: s.cmd.do("_ clip slab,30"))


        self.menuBar.addmenuitem('Display', 'separator', '')

        self.menuBar.addcascademenu('Display', 'Background', 'Background',
                                             label='Background')

        self.menuBar.addmenuitem('Background', 'checkbutton',
                                 'Opaque Background Color',
                                 label='Opaque',
                                variable = self.setting.opaque_background,
                                )

        self.menuBar.addmenuitem('Background', 'checkbutton',
                                 'Show Alpha Checker',
                                 label='Show Alpha Checker',
                                variable = self.setting.show_alpha_checker,
                                )

        self.menuBar.addmenuitem('Background', 'separator', '')
        
        self.menuBar.addmenuitem('Background', 'command', 'White Background',
                                         label='White',
                                         command = lambda s=self: s.cmd.do("_ cmd.bg_color('white')"))

        self.menuBar.addmenuitem('Background', 'command', 'Light Grey',
                                         label='Light Grey',
                                         command = lambda s=self: s.cmd.do("_ cmd.bg_color('grey80')"))

        self.menuBar.addmenuitem('Background', 'command', 'Grey Background',
                                         label='Grey',
                                         command = lambda s=self: s.cmd.do("_ cmd.bg_color('grey50')"))

        self.menuBar.addmenuitem('Background', 'command', 'Black Background',
                                         label='Black',
                                         command = lambda s=self: s.cmd.do("_ cmd.bg_color('black')"))


        self.menuBar.addcascademenu('Display', 'Color Space', 'Color Space',
                                             label='Color Space')

        self.menuBar.addmenuitem('Color Space', 'command', 'CMYK (for publications)',
                                         label='CMYK (for publications)',
                                         command = lambda s=self: s.cmd.do("_ cmd.space('cmyk')"))

        self.menuBar.addmenuitem('Color Space', 'command', 'PyMOL (for video & web)',
                                         label='PyMOL (for video & web)',
                                         command = lambda s=self: s.cmd.do("_ cmd.space('pymol')"))

        self.menuBar.addmenuitem('Color Space', 'command', 'RGB (default)',
                                         label='RGB (default)',
                                         command = lambda s=self: s.cmd.do("_ cmd.space('rgb')"))

        self.menuBar.addcascademenu('Display', 'Performance', 'Quality',
                                             label='Quality')

        self.menuBar.addmenuitem('Performance', 'command', 'Maximum Performance',
                                         label='Maximum Performance',
                                         command = lambda s=self: s.cmd.do("_ util.performance(100)"))

        self.menuBar.addmenuitem('Performance', 'command', 'Reasonable Performance',
                                         label='Reasonable Performance',
                                         command = lambda s=self: s.cmd.do("_ util.performance(66)"))
        
        self.menuBar.addmenuitem('Performance', 'command', 'Reasonable Quality',
                                         label='Reasonable Quality',
                                         command = lambda s=self: s.cmd.do("_ util.performance(33)"))

        self.menuBar.addmenuitem('Performance', 'command', 'Maximum Quality',
                                         label='Maximum Quality',
                                         command = lambda s=self: s.cmd.do("_ util.performance(0)"))


        self.menuBar.addcascademenu('Display', 'Grid', 'Grid',
                                             label='Grid')

        self.menuBar.addmenuitem('Grid', 'command', 'By Object',
                                         label='By Object',
                                         command = lambda s=self: s.cmd.do("_ set grid_mode, 1"))

        self.menuBar.addmenuitem('Grid', 'command', 'By State',
                                         label='By State',
                                         command = lambda s=self: s.cmd.do("_ set grid_mode, 2"))
                                 
        self.menuBar.addmenuitem('Grid', 'command', 'Disable',
                                         label='Disable',
                                         command = lambda s=self: s.cmd.do("_ set grid_mode, 0"))
        
        self.menuBar.addmenuitem('Display', 'separator', '')
        
        self.menuBar.addmenuitem('Display', 'checkbutton',
                                 'Disable perspective.',
                                 label='Orthoscopic View',
                                variable = self.setting.ortho)


        self.menuBar.addmenuitem('Display', 'checkbutton',
                                 'Show Valences.',
                                 label='Show Valences',
                                variable = self.setting.valence,
                                )


        self.menuBar.addmenuitem('Display', 'checkbutton',
                                 'Smooth Lines.',
                                 label='Smooth Lines',
                                variable = self.setting.line_smooth,
                                )

        self.menuBar.addmenuitem('Display', 'checkbutton',
                                 'Depth Cue (Fogging).',
                                 label='Depth Cue',
                                variable = self.setting.depth_cue,
                                )

        self.menuBar.addmenuitem('Display', 'checkbutton',
                                 'Two Sided Lighting.',
                                 label='Two Sided Lighting',
                                variable = self.setting.two_sided_lighting,
                                )

        self.menuBar.addmenuitem('Display', 'checkbutton',
                                 'Specular Reflections.',
                                 label='Specular Reflections',
                                variable = self.setting.specular,
                                onvalue=1.0, offvalue=0.0,
                                )

        self.menuBar.addmenuitem('Display', 'checkbutton',
                                 'Use Display Lists.',
                                 label='Use Display Lists',
                                variable = self.setting.use_display_lists,
                                )

        self.menuBar.addmenuitem('Display', 'checkbutton',
                                 'Animation',
                                 label='Animation',
                                variable = self.setting.animation,
                                )

        self.menuBar.addmenuitem('Display', 'checkbutton',
                                 'Roving Detail',
                                 label='Roving Detail',
                                variable = self.setting.roving_detail,
                                )

        self.menuBar.addmenu('Setting', 'Settings and Configuration',tearoff=TRUE)

        self.menuBar.addmenuitem('Setting', 'command',
                                 'Edit PyMOL Settings',
                                 label='Edit All...',
                                         command = lambda s=self: SetEditor(s))

        self.menuBar.addmenuitem('Setting', 'command',
                                 'Edit PyMOL Colors',
                                 label='Colors...',
                                         command = lambda s=self: ColorEditor(s))

        self.menuBar.addcascademenu('Setting', 'Label', 'Label',
                                             label='Label')

        self.menuBar.addcascademenu('Label', 'LabelSize', 'Size',
                                             label='Size',tearoff=TRUE)

        self.menuBar.addmenuitem('LabelSize', 'command', '10 Point',
                                 label='10 Point',
                                 command = lambda s=self: s.cmd.do("_ cmd.set('label_size', 10)"))

        self.menuBar.addmenuitem('LabelSize', 'command', '14 Point',
                                 label='14 Point',
                                 command = lambda s=self: s.cmd.do("_ cmd.set('label_size', 14)"))

        self.menuBar.addmenuitem('LabelSize', 'command', '18 Point',
                                 label='18 Point',
                                 command = lambda s=self: s.cmd.do("_ cmd.set('label_size', 18)"))

        self.menuBar.addmenuitem('LabelSize', 'command', '24 Point',
                                 label='24 Point',
                                 command = lambda s=self: s.cmd.do("_ cmd.set('label_size', 24)"))

        self.menuBar.addmenuitem('LabelSize', 'command', '36 Point',
                                 label='36 Point',
                                 command = lambda s=self: s.cmd.do("_ cmd.set('label_size', 36)"))

        self.menuBar.addmenuitem('LabelSize', 'command', '48 Point',
                                 label='48 Point',
                                 command = lambda s=self: s.cmd.do("_ cmd.set('label_size', 48)"))

        self.menuBar.addmenuitem('LabelSize', 'command', '72 Point',
                                 label='72 Point',
                                 command = lambda s=self: s.cmd.do("_ cmd.set('label_size', 72)"))

        self.menuBar.addmenuitem('LabelSize', 'separator', '')

        self.menuBar.addmenuitem('LabelSize', 'command', '0.3 Angstrom',
                                 label='0.3 Angstrom',
                                 command = lambda s=self: s.cmd.do("_ cmd.set('label_size', -0.3)"))

        self.menuBar.addmenuitem('LabelSize', 'command', '0.5 Angstrom',
                                 label='0.5 Angstrom',
                                 command = lambda s=self: s.cmd.do("_ cmd.set('label_size', -0.5)"))

        self.menuBar.addmenuitem('LabelSize', 'command', '1 Angstrom',
                                 label='1 Angstrom',
                                 command = lambda s=self: s.cmd.do("_ cmd.set('label_size', -1)"))

        self.menuBar.addmenuitem('LabelSize', 'command', '2 Angstrom',
                                 label='2 Angstrom',
                                 command = lambda s=self: s.cmd.do("_ cmd.set('label_size', -2)"))

        self.menuBar.addmenuitem('LabelSize', 'command', '4 Angstrom',
                                 label='4 Angstrom',
                                 command = lambda s=self: s.cmd.do("_ cmd.set('label_size', -4)"))


        self.menuBar.addcascademenu('Label', 'LabelFont', 'Font',
                                    label='Font', tearoff=TRUE)
        
        self.menuBar.addmenuitem('LabelFont', 'command', 'DejaVuSans',
                                 label='DevaVuSans',
                                 command = lambda s=self: s.cmd.do("_ cmd.set('label_font_id',5)"))

        self.menuBar.addmenuitem('LabelFont', 'command', 'DejaVuSans Oblique',
                                 label='DevaVuSans Oblique',
                                 command = lambda s=self: s.cmd.do("_ cmd.set('label_font_id',6)"))

        self.menuBar.addmenuitem('LabelFont', 'command', 'DejaVuSans Bold',
                                 label='DevaVuSans Bold',
                                 command = lambda s=self: s.cmd.do("_ cmd.set('label_font_id',7)"))

        self.menuBar.addmenuitem('LabelFont', 'command', 'DejaVuSans Bold Oblique',
                                 label='DevaVuSans Bold Oblique',
                                 command = lambda s=self: s.cmd.do("_ cmd.set('label_font_id',8)"))

        self.menuBar.addmenuitem('LabelFont', 'command', 'DejaVuSerif',
                                 label='DevaVuSerif',
                                 command = lambda s=self: s.cmd.do("_ cmd.set('label_font_id',9)"))

        self.menuBar.addmenuitem('LabelFont', 'command', 'DejaVuSerif Bold',
                                 label='DevaVuSerif Bold',
                                 command = lambda s=self: s.cmd.do("_ cmd.set('label_font_id',10)"))

        self.menuBar.addmenuitem('LabelFont', 'command', 'DejaVuSerif Oblique',
                                 label='DevaVuSerif Oblique',
                                 command = lambda s=self: s.cmd.do("_ cmd.set('label_font_id',17)"))

        self.menuBar.addmenuitem('LabelFont', 'command', 'DejaVuSerif Bold Oblique',
                                 label='DevaVuSerif Bold Oblique',
                                 command = lambda s=self: s.cmd.do("_ cmd.set('label_font_id',18)"))

        self.menuBar.addmenuitem('LabelFont', 'command', 'DejaVuSansMono',
                                 label='DevaVuSansMono',
                                 command = lambda s=self: s.cmd.do("_ cmd.set('label_font_id',11)"))

        self.menuBar.addmenuitem('LabelFont', 'command', 'DejaVuSansMono Oblique',
                                 label='DevaVuSansMono Oblique',
                                 command = lambda s=self: s.cmd.do("_ cmd.set('label_font_id',12)"))

        self.menuBar.addmenuitem('LabelFont', 'command', 'DejaVuSansMono Bold',
                                 label='DevaVuSansMono Bold',
                                 command = lambda s=self: s.cmd.do("_ cmd.set('label_font_id',13)"))

        self.menuBar.addmenuitem('LabelFont', 'command', 'DejaVuSansMono Bold Oblique',
                                 label='DevaVuSansMono Bold Oblique',
                                 command = lambda s=self: s.cmd.do("_ cmd.set('label_font_id',14)"))

        self.menuBar.addmenuitem('LabelFont', 'command', 'Gentium Roman',
                                 label='Gentium Roman',
                                 command = lambda s=self: s.cmd.do("_ cmd.set('label_font_id',15)"))

        self.menuBar.addmenuitem('LabelFont', 'command', 'Gentium Italic',
                                 label='Gentium Italic',
                                 command = lambda s=self: s.cmd.do("_ cmd.set('label_font_id',16)"))

        self.menuBar.addcascademenu('Setting', 'Cartoon', 'Cartoon',
                                             label='Cartoon')

        self.menuBar.addcascademenu('Cartoon', 'Rings', 'Rings & Bases',
                                             label='Rings & Bases')

        self.menuBar.addmenuitem('Rings', 'command', 'Filled Rings (Round Edges)',
                                 label='Filled Rings (Round Edges)',
                                 command = lambda s=self: s.cmd.do("_ cmd.set('cartoon_ring_mode', 1)"))

        self.menuBar.addmenuitem('Rings', 'command', 'Filled Rings (Flat Edges)',
                                 label='Filled Rings (Flat Edges)',
                                 command = lambda s=self: s.cmd.do("_ cmd.set('cartoon_ring_mode', 2)"))

        self.menuBar.addmenuitem('Rings', 'command', 'Filled Rings (With Border)',
                                 label='Filled Rings (with Border)',
                                 command = lambda s=self: s.cmd.do("_ cmd.set('cartoon_ring_mode', 3)"))

        self.menuBar.addmenuitem('Rings', 'command', 'Spheres',
                                 label='Spheres',
                                 command = lambda s=self: s.cmd.do("_ cmd.set('cartoon_ring_mode', 4)"))

        self.menuBar.addmenuitem('Rings', 'command', 'Base Ladders',
                                 label='Base Ladders',
                                 command = lambda s=self: s.cmd.do("_ cmd.set('cartoon_ring_mode', 0)"))

        self.menuBar.addmenuitem('Rings', 'separator', '')

        self.menuBar.addmenuitem('Rings', 'command', 'Bases & Sugars',
                                 label='Bases & Sugars',
                                 command = lambda s=self: s.cmd.do("_ cmd.set('cartoon_ring_finder', 1)"))

        self.menuBar.addmenuitem('Rings', 'command', 'Bases Only',
                                 label='Bases Only',
                                 command = lambda s=self: s.cmd.do("_ cmd.set('cartoon_ring_finder', 2)"))


        self.menuBar.addmenuitem('Rings', 'command', 'Non-protein Rings',
                                 label='Non-protein Rings',
                                 command = lambda s=self: s.cmd.do("_ cmd.set('cartoon_ring_finder', 3)"))

        self.menuBar.addmenuitem('Rings', 'command', 'All Rings',
                                 label='All Rings',
                                 command = lambda s=self: s.cmd.do("_ cmd.set('cartoon_ring_finder', 4)"))

        self.menuBar.addmenuitem('Cartoon', 'checkbutton',
                                 'Side Chain Helper',
                                 label='Side Chain Helper',
                                variable = self.setting.cartoon_side_chain_helper,
                                )

        self.menuBar.addmenuitem('Rings', 'separator', '')

        self.menuBar.addmenuitem('Rings', 'command', 'Transparent Rings',
                                 label='Transparent Rings',
                                 command = lambda s=self: s.cmd.do("_ cmd.set('cartoon_ring_transparency', 0.5)"))

        self.menuBar.addmenuitem('Rings', 'command', 'Default',
                                 label='Default',
                                 command = lambda s=self: s.cmd.do("_ cmd.set('cartoon_ring_transparency', -1)"))

        self.menuBar.addmenuitem('Cartoon', 'checkbutton',
                                 'Round Helices',
                                 label='Round Helices',
                                variable = self.setting.cartoon_round_helices,
                                )

        self.menuBar.addmenuitem('Cartoon', 'checkbutton',
                                 'Fancy Helices',
                                 label='Fancy Helices',
                                variable = self.setting.cartoon_fancy_helices,
                                )

        self.menuBar.addmenuitem('Cartoon', 'checkbutton',
                                 'Cylindrical Helices',
                                 label='Cylindrical Helices',
                                variable = self.setting.cartoon_cylindrical_helices,
                                )

        self.menuBar.addmenuitem('Cartoon', 'checkbutton',
                                 'Flat Sheets',
                                 label='Flat Sheets',
                                variable = self.setting.cartoon_flat_sheets,
                                )


        self.menuBar.addmenuitem('Cartoon', 'checkbutton',
                                 'Fancy Sheets',
                                 label='Fancy Sheets',
                                variable = self.setting.cartoon_fancy_sheets,
                                )

        self.menuBar.addmenuitem('Cartoon', 'checkbutton',
                                 'Smooth Loops',
                                 label='Smooth Loops',
                                variable = self.setting.cartoon_smooth_loops,
                                )

        self.menuBar.addmenuitem('Cartoon', 'checkbutton',
                                 'Discrete Colors',
                                 label='Discrete Colors',
                                variable = self.setting.cartoon_discrete_colors,
                                )

        self.menuBar.addmenuitem('Cartoon', 'checkbutton',
                                 'Highlight Color',
                                 label='Highlight Color',
                                variable = self.setting.cartoon_highlight_color,
                                onvalue=104, offvalue=-1,
                                )

        self.menuBar.addcascademenu('Setting', 'Ribbon', 'Ribbon',
                                             label='Ribbon')

        self.menuBar.addmenuitem('Ribbon', 'checkbutton',
                                 'Side Chain Helper',
                                 label='Side Chain Helper',
                                variable = self.setting.ribbon_side_chain_helper,
                                )
        
        self.menuBar.addmenuitem('Ribbon', 'checkbutton',
                                 'Trace Atoms',
                                 label='Trace Atoms',
                                variable = self.setting.ribbon_trace_atoms,
                                )

        self.menuBar.addcascademenu('Setting', 'Surface', 'Surface',
                                             label='Surface')

        self.menuBar.addcascademenu('Surface', 'Surface Color', 'Color',
                                    label='Color')

        self.menuBar.addmenuitem('Surface Color', 'command', 'White',
                                 label='White',
                                 command = lambda s=self:
                                 s.cmd.do("_ cmd.set('surface_color', 'white')"))

        self.menuBar.addmenuitem('Surface Color', 'command', 'Light Grey',
                                 label='Light Grey',
                                 command = lambda s=self: s.cmd.do("_ cmd.set('surface_color', 'grey80')"))
        
        self.menuBar.addmenuitem('Surface Color', 'command', 'Grey',
                                 label='Grey',
                                 command = lambda s=self:
                                 s.cmd.do("_ cmd.set('surface_color', 'grey')"))

        self.menuBar.addmenuitem('Surface Color', 'command', 'Default (Atomic)',
                                 label='Default (Atomic)',
                                 command = lambda s=self:
                                 s.cmd.do("_ cmd.set('surface_color', -1)"))

        self.menuBar.addmenuitem('Surface', 'command', 'Dot',
                                 label='Dot',
                                 command = lambda s=self:
                                 s.cmd.do("_ cmd.set('surface_type',1)"))

        self.menuBar.addmenuitem('Surface', 'command', 'Wireframe',
                                 label='Wireframe',
                                 command = lambda s=self:
                                 s.cmd.do("_ cmd.set('surface_type',2)"))

        self.menuBar.addmenuitem('Surface', 'command', 'Solid',
                                 label='Solid',
                                 command = lambda s=self:
                                 s.cmd.do("_ cmd.set('surface_type',0)"))

        self.menuBar.addmenuitem('Surface', 'separator', '')
        
        self.menuBar.addmenuitem('Surface', 'command', 'Cavities & Pockets Only',
                                 label='Cavities & Pockets Only',
                                 command = lambda s=self:
                                 s.cmd.do("_ cmd.set('surface_cavity_mode',1)"))

        self.menuBar.addmenuitem('Surface', 'command', 'Cavities & Pockets (Culled)',
                                 label='Cavities & Pockets (Culled)',
                                 command = lambda s=self:
                                 s.cmd.do("_ cmd.set('surface_cavity_mode',2)"))

        self.menuBar.addcascademenu('Surface', 'Detection', 'Cavity Detection Radius',
                                    label='Cavity Detection Radius')

        self.menuBar.addmenuitem('Detection', 'command', '3 Solvent Radii',
                                 label='3 Solvent Radii',
                                 command = lambda s=self:
                                 s.cmd.do("_ cmd.set('surface_cavity_radius',-3)"))

        self.menuBar.addmenuitem('Detection', 'command', '4 Solvent Radii',
                                 label='4 Solvent Radii',
                                 command = lambda s=self:
                                 s.cmd.do("_ cmd.set('surface_cavity_radius',-4)"))

        self.menuBar.addmenuitem('Detection', 'command', '5 Solvent Radii',
                                 label='5 Solvent Radii',
                                 command = lambda s=self:
                                 s.cmd.do("_ cmd.set('surface_cavity_radius',-5)"))

        self.menuBar.addmenuitem('Detection', 'command', '6 Solvent Radii',
                                 label='6 Solvent Radii',
                                 command = lambda s=self:
                                 s.cmd.do("_ cmd.set('surface_cavity_radius',-6)"))

        self.menuBar.addmenuitem('Detection', 'command', '8 Solvent Radii',
                                 label='8 Solvent Radii',
                                 command = lambda s=self:
                                 s.cmd.do("_ cmd.set('surface_cavity_radius',-8)"))

        self.menuBar.addmenuitem('Detection', 'command', '10 Solvent Radii',
                                 label='10 Solvent Radii',
                                 command = lambda s=self:
                                 s.cmd.do("_ cmd.set('surface_cavity_radius',-10)"))

        self.menuBar.addmenuitem('Detection', 'command', '20 Solvent Radii',
                                 label='20 Solvent Radii',
                                 command = lambda s=self:
                                 s.cmd.do("_ cmd.set('surface_cavity_radius',-20)"))

        self.menuBar.addcascademenu('Surface', 'Cutoff', 'Cavity Detection Cutoff',
                                    label='Cavity Detection Cutoff')

        self.menuBar.addmenuitem('Cutoff', 'command', '1 Solvent Radius',
                                 label='1 Solvent Radius',
                                 command = lambda s=self:
                                 s.cmd.do("_ cmd.set('surface_cavity_cutoff',-1)"))

        self.menuBar.addmenuitem('Cutoff', 'command', '2 Solvent Radii',
                                 label='2 Solvent Radii',
                                 command = lambda s=self:
                                 s.cmd.do("_ cmd.set('surface_cavity_cutoff',-2)"))


        self.menuBar.addmenuitem('Cutoff', 'command', '3 Solvent Radii',
                                 label='3 Solvent Radii',
                                 command = lambda s=self:
                                 s.cmd.do("_ cmd.set('surface_cavity_cutoff',-3)"))

        self.menuBar.addmenuitem('Cutoff', 'command', '4 Solvent Radii',
                                 label='4 Solvent Radii',
                                 command = lambda s=self:
                                 s.cmd.do("_ cmd.set('surface_cavity_cutoff',-4)"))

        self.menuBar.addmenuitem('Cutoff', 'command', '5 Solvent Radii',
                                 label='5 Solvent Radii',
                                 command = lambda s=self:
                                 s.cmd.do("_ cmd.set('surface_cavity_cutoff',-5)"))

        self.menuBar.addmenuitem('Surface', 'command', 'Exterior (Normal)',
                                 label='Exterior (Normal)',
                                 command = lambda s=self:
                                 s.cmd.do("_ cmd.set('surface_cavity_mode',0)"))

        self.menuBar.addmenuitem('Surface', 'separator', '')

        self.menuBar.addmenuitem('Surface', 'checkbutton',
                                 'Solvent Accessible',
                                 label='Solvent Accessible',
                                 variable = self.setting.surface_solvent,
                                 )

        self.menuBar.addmenuitem('Surface', 'separator', '')
        
        self.menuBar.addmenuitem('Surface', 'command', 'Ignore None',
                                 label='Ignore None',
                                 command = lambda s=self:
                                 s.cmd.do("_ cmd.set('surface_mode',1)"))

        self.menuBar.addmenuitem('Surface', 'command', 'Ignore HETATMs',
                                 label='Ignore HETATMs',
                                 command = lambda s=self:
                                 s.cmd.do("_ cmd.set('surface_mode',0)"))

        self.menuBar.addmenuitem('Surface', 'command', 'Ignore Hydrogens',
                                 label='Ignore Hydrogens',
                                 command = lambda s=self:
                                 s.cmd.do("_ cmd.set('surface_mode',2)"))

        self.menuBar.addmenuitem('Surface', 'command', 'Ignore Unsurfaced',
                                 label='Ignore Unsurfaced',
                                 command = lambda s=self:
                                 s.cmd.do("_ cmd.set('surface_mode',3)"))

        self.menuBar.addcascademenu('Setting', 'Volume', label='Volume')

        self.menuBar.addmenuitem('Volume', 'checkbutton', label='Pre-integrated Rendering',
                                state='disabled',
                                variable = self.setting.volume_mode)

        self.menuBar.addcascademenu('Volume', 'VolumeLayers', label='Number of Layers')

        for i in (100., 256., 500., 1000.):
            self.menuBar.addmenuitem('VolumeLayers', 'radiobutton', label='%.0f' % i,
                    value=i, variable=self.setting.volume_layers)

        self.menuBar.addcascademenu('Setting', 'Transparency', 'Transparency',
                                             label='Transparency')

        self.transparency_menu('CartoonTransparency','Cartoon','cartoon_transparency')
        self.transparency_menu('SurfaceTransparency','Surface','transparency')
        self.transparency_menu('StickTransparency','Stick','stick_transparency')
        self.transparency_menu('SphereTransparency','Sphere','sphere_transparency')      

        self.menuBar.addmenuitem('Transparency', 'separator', '')
        

        self.menuBar.addmenuitem('Transparency', 'command', 'Uni-Layer',
                                         label='Uni-Layer',
                                         command = lambda s=self: s.cmd.do(
            "_ cmd.set('transparency_mode',2);cmd.set('backface_cull',1);cmd.set('two_sided_lighting',0)"))

        self.menuBar.addmenuitem('Transparency', 'command', 'Multi-Layer',
                                         label='Multi-Layer',
                                         command = lambda s=self: s.cmd.do(
            "_ cmd.set('transparency_mode',1);cmd.set('backface_cull',0);cmd.set('two_sided_lighting',1)"))

        self.menuBar.addmenuitem('Transparency', 'command', 'Fast and Ugly',
                                         label='Fast and Ugly',
                                         command = lambda s=self: s.cmd.do(
            "_ cmd.set('transparency_mode',0);cmd.set('backface_cull',1);cmd.set('two_sided_lighting',0)"))

                
        self.menuBar.addcascademenu('Setting', 'Rendering', 'Rendering',
                                             label='Rendering')

        self.menuBar.addmenuitem('Rendering', 'checkbutton',
                                 'Smooth raytracing.',
                                 label='Antialias',
                                variable = self.setting.antialias,
                                )

        self.menuBar.addmenuitem('Rendering', 'command', 'Modernize',
                                 label='Modernize',
                                 command = lambda s=self: s.util.modernize_rendering(1,s.cmd))

        self.menuBar.addmenuitem('Rendering', 'separator', '')
        
        self.menuBar.addcascademenu('Rendering', 'Shadows', 'Shadows',
                                         label='Shadows')

        self.menuBar.addmenuitem('Shadows', 'command', 'None',
                                         label='None',
                                         command = lambda s=self: s.cmd.do("_ util.ray_shadows('none')"))

        self.menuBar.addmenuitem('Shadows', 'command', 'Light',
                                         label='Light',
                                         command = lambda s=self: s.cmd.do("_ util.ray_shadows('light')"))

        self.menuBar.addmenuitem('Shadows', 'command', 'Medium',
                                         label='Medium',
                                         command = lambda s=self: s.cmd.do("_ util.ray_shadows('medium')"))

        self.menuBar.addmenuitem('Shadows', 'command', 'Heavy',
                                         label='Heavy',
                                         command = lambda s=self: s.cmd.do("_ util.ray_shadows('heavy')"))

        self.menuBar.addmenuitem('Shadows', 'command', 'Black',
                                         label='Black',
                                         command = lambda s=self: s.cmd.do("_ util.ray_shadows('black')"))

        self.menuBar.addmenuitem('Shadows', 'separator', '')
        
        self.menuBar.addmenuitem('Shadows', 'command', 'Matte',
                                         label='Matte',
                                         command = lambda s=self: s.cmd.do("_ util.ray_shadows('matte')"))
        self.menuBar.addmenuitem('Shadows', 'command', 'Soft',
                                         label='Soft',
                                         command = lambda s=self: s.cmd.do("_ util.ray_shadows('soft')"))

        self.menuBar.addmenuitem('Shadows', 'command', 'Occlusion',
                                        label='Occlusion',
                                         command = lambda s=self: s.cmd.do("_ util.ray_shadows('occlusion')"))

        self.menuBar.addmenuitem('Shadows', 'command', 'Occlusion 2',
                                        label='Occlusion 2',
                                         command = lambda s=self: s.cmd.do("_ util.ray_shadows('occlusion2')"))


        self.menuBar.addcascademenu('Rendering', 'Texture', 'Texture',
                                         label='Texture')

        self.menuBar.addmenuitem('Texture', 'command', 'None',
                                         label='None',
                                         command = lambda s=self: s.cmd.do("_ cmd.set('ray_texture',0)"))

        self.menuBar.addmenuitem('Texture', 'command', 'Matte 1',
                                         label='Matte 1',
                                         command = lambda s=self: s.cmd.do("_ cmd.set('ray_texture',1)"))

        self.menuBar.addmenuitem('Texture', 'command', 'Matte 2',
                                         label='Matte 2',
                                         command = lambda s=self: s.cmd.do("_ cmd.set('ray_texture',4)"))

        self.menuBar.addmenuitem('Texture', 'command', 'Swirl 1',
                                         label='Swirl 1',
                                         command = lambda s=self: s.cmd.do("_ cmd.set('ray_texture',2)"))

        self.menuBar.addmenuitem('Texture', 'command', 'Swirl 2',
                                         label='Swirl 2',
                                         command = lambda s=self: s.cmd.do("_ cmd.set('ray_texture',3)"))

        self.menuBar.addmenuitem('Texture', 'command', 'Fiber',
                                         label='Fiber',
                                         command = lambda s=self: s.cmd.do("_ cmd.set('ray_texture',5)"))

        self.menuBar.addcascademenu('Rendering', 'Interior Texture', 'Interior Texture',
                                         label='Interior Texture')

        self.menuBar.addmenuitem('Interior Texture', 'command', 'None',
                                         label='None',
                                         command = lambda s=self: s.cmd.do("_ cmd.set('ray_interior_texture',0)"))

        self.menuBar.addmenuitem('Interior Texture', 'command', 'Matte 1',
                                         label='Matte 1',
                                         command = lambda s=self: s.cmd.do("_ cmd.set('ray_interior_texture',1)"))

        self.menuBar.addmenuitem('Interior Texture', 'command', 'Matte 2',
                                         label='Matte 2',
                                         command = lambda s=self: s.cmd.do("_ cmd.set('ray_interior_texture',4)"))

        self.menuBar.addmenuitem('Interior Texture', 'command', 'Swirl 1',
                                         label='Swirl 1',
                                         command = lambda s=self: s.cmd.do("_ cmd.set('ray_interior_texture',2)"))

        self.menuBar.addmenuitem('Interior Texture', 'command', 'Swirl 2',
                                         label='Swirl 2',
                                         command = lambda s=self: s.cmd.do("_ cmd.set('ray_interior_texture',3)"))

        self.menuBar.addmenuitem('Interior Texture', 'command', 'Fiber',
                                         label='Fiber',
                                         command = lambda s=self: s.cmd.do("_ cmd.set('ray_interior_texture',5)"))


        self.menuBar.addcascademenu('Rendering', 'Memory', 'Memory',
                                         label='Memory')

        self.menuBar.addmenuitem('Memory', 'command', 'Use Less (slower)',
                                         label='Use Less (slower)',
                                         command = lambda s=self: s.cmd.do("_ cmd.set('hash_max',70,quiet=0)"))

        self.menuBar.addmenuitem('Memory', 'command', 'Use Standard Amount',
                                         label='Use Standard Amount',
                                         command = lambda s=self: s.cmd.do("_ cmd.set('hash_max',100,quiet=0)"))

        self.menuBar.addmenuitem('Memory', 'command', 'Use More (faster)',
                                         label='Use More (faster)',
                                         command = lambda s=self: s.cmd.do("_ cmd.set('hash_max',170,quiet=0)"))

        self.menuBar.addmenuitem('Memory', 'command', 'Use Even More',
                                         label='Use Even More',
                                         command = lambda s=self: s.cmd.do("_ cmd.set('hash_max',230,quiet=0)"))

        self.menuBar.addmenuitem('Memory', 'command', 'Use Most',
                                         label='Use Most',
                                         command = lambda s=self: s.cmd.do("_ cmd.set('hash_max',300,quiet=0)"))

        self.menuBar.addmenuitem('Rendering', 'separator', '')

        self.menuBar.addmenuitem('Rendering', 'checkbutton',
                                 'Cull Backfaces when Rendering',
                                 label='Cull Backfaces',
                                variable = self.setting.backface_cull,
                                )


        self.menuBar.addmenuitem('Rendering', 'checkbutton',
                                 'Opaque Interior Colors',
                                 label='Opaque Interiors',
                                variable = self.setting.ray_interior_color,
                                onvalue=74, offvalue=-1,
                                )

        self.menuBar.addmenuitem('Setting', 'separator', '')

        self.menuBar.addcascademenu('Setting', 'Output', 'Output Size',
                                             label='Output Size')

        self.menuBar.addmenuitem('Output', 'command', '8',
                                         label='8 Point',
                                         command = lambda s=self:
                                         s.text.configure(font=(s.font,8)))

        self.menuBar.addmenuitem('Output', 'command', '9',
                                         label='9 Point',
                                         command = lambda s=self:
                                         s.text.configure(font=(s.font,9)))

        self.menuBar.addmenuitem('Output', 'command', '10',
                                         label='10 Point',
                                         command = lambda s=self:
                                         s.text.configure(font=(s.font,10)))

        self.menuBar.addmenuitem('Output', 'command', '11',
                                         label='11 Point',
                                         command = lambda s=self:
                                         s.text.configure(font=(s.font,11)))

        self.menuBar.addmenuitem('Output', 'command', '12',
                                         label='12 Point',
                                         command = lambda s=self:
                                         s.text.configure(font=(s.font,12)))

        self.menuBar.addcascademenu('Setting', 'Control', 'Control Size',
                                             label='Control Size')

        self.menuBar.addmenuitem('Control', 'command', '12',
                                         label='12',
                                         command = lambda s=self:
                                         s.cmd.do("_ set internal_gui_control_size,12,quiet=1"))

        self.menuBar.addmenuitem('Control', 'command', '14',
                                         label='14',
                                         command = lambda s=self:
                                         s.cmd.do("_ set internal_gui_control_size,14,quiet=1"))

        self.menuBar.addmenuitem('Control', 'command', '16',
                                         label='16',
                                         command = lambda s=self:
                                         s.cmd.do("_ set internal_gui_control_size,16,quiet=1"))

        self.menuBar.addmenuitem('Control', 'command', '18',
                                         label='18 (default)',
                                         command = lambda s=self:
                                         s.cmd.do("_ set internal_gui_control_size,18,quiet=1"))

        self.menuBar.addmenuitem('Control', 'command', '20',
                                         label='20',
                                         command = lambda s=self:
                                         s.cmd.do("_ set internal_gui_control_size,20,quiet=1"))

        self.menuBar.addmenuitem('Control', 'command', '24',
                                         label='24',
                                         command = lambda s=self:
                                         s.cmd.do("_ set internal_gui_control_size,24,quiet=1"))

        self.menuBar.addmenuitem('Control', 'command', '30',
                                         label='30',
                                         command = lambda s=self:
                                         s.cmd.do("_ set internal_gui_control_size,30,quiet=1"))

        self.menuBar.addmenuitem('Setting', 'separator', '')
        
        
        self.menuBar.addmenuitem('Setting', 'checkbutton',
                                         'Ignore PDB segi.',
                                         label='Ignore PDB Segment Identifier',
                                         variable = self.setting.ignore_pdb_segi,
                                         )

        self.menuBar.addmenuitem('Setting', 'checkbutton',
                                 'Auto-Zoom.',
                                 label='Auto-Zoom New Objects',
                                variable = self.setting.auto_zoom,
                                )

        self.menuBar.addmenuitem('Setting', 'checkbutton',
                                 'Auto-Show Selections.',
                                 label='Auto-Show New Selections',
                                variable = self.setting.auto_show_selections,
                                )

        self.menuBar.addmenuitem('Setting', 'checkbutton',
                                 'Auto-Hide Selections.',
                                 label='Auto-Hide Selections',
                                variable = self.setting.auto_hide_selections,
                                )

        self.menuBar.addmenuitem('Setting', 'checkbutton',
                                 'Auto-Remove Hydrogens.',
                                 label='Auto-Remove Hydrogens',
                                variable = self.setting.auto_remove_hydrogens,
                                )

        self.menuBar.addmenuitem('Setting', 'separator', '')


        self.menuBar.addmenuitem('Setting', 'command', 'Show Text Output',
                                         label='Show Text',
                                         command = lambda s=self: s.cmd.set("text","1",log=1))

        self.menuBar.addmenuitem('Setting', 'command', 'Hide Text Output',
                                         label='Hide Text',
                                         command = lambda s=self: s.cmd.set("text","0",log=1))

        self.menuBar.addmenuitem('Setting', 'checkbutton',
                                 'Overlay Text Output on Graphics',
                                 label='Overlay Text',
                                variable = self.setting.overlay,
                                )

        self.menuBar.addmenu('Scene', 'Scene Storage',tearoff=TRUE)

        self.menuBar.addmenuitem('Scene', 'command', 'Next',
                                         label='Next [PgDn]',
                                         command = lambda s=self: s.cmd.scene('auto','next'))

        self.menuBar.addmenuitem('Scene', 'command', 'Previous',
                                         label='Previous [PgUp]',
                                         command = lambda s=self: s.cmd.scene('auto','previous'))

        self.menuBar.addmenuitem('Scene', 'separator', '')
        
        self.menuBar.addmenuitem('Scene', 'command', 'Append',
                                         label='Append',
                                         command = lambda s=self: s.cmd.scene('new','store'))

        self.menuBar.addmenuitem('Scene', 'command', 'Insert Before',
                                         label='Insert (before)',
                                         command = lambda s=self: s.cmd.scene('','insert_before'))

        self.menuBar.addmenuitem('Scene', 'command', 'Insert After',
                                         label='Insert (after)',
                                         command = lambda s=self: s.cmd.scene('','insert_after'))

        self.menuBar.addmenuitem('Scene', 'command', 'Update',
                                         label='Update',
                                         command = lambda s=self: s.cmd.scene('auto','update'))

#      self.menuBar.addmenuitem('Scene', 'command', 'Annotate',
#                               label='Append',
#                               command = lambda s=self: s.cmd.scene('new','store'))


        self.menuBar.addmenuitem('Scene', 'separator', '')

        self.menuBar.addmenuitem('Scene', 'command', 'Delete',
                                         label='Delete',
                                         command = lambda s=self: s.cmd.scene('auto','clear'))

        self.menuBar.addmenuitem('Scene', 'separator', '')

        self.menuBar.addcascademenu('Scene', 'Recall', 'Recall',
                                             label='Recall')

        self.menuBar.addcascademenu('Scene', 'Store', 'Store',
                                             label='Store')

#      self.menuBar.addcascademenu('Store', 'StoreSHFT', 'StoreSHFT',
#                                  label='Shift')

        self.menuBar.addcascademenu('Scene', 'Clear', 'Clear',
                                             label='Clear')

#      self.menuBar.addcascademenu('Scene', 'SceneSHFT', 'SceneSHFT',
#                                  label='Shift')

        for x in range(1,13):
            self.menuBar.addmenuitem('Store', 'checkbutton', 'F%d'%x,
                                             label='F%d'%x,
                                             variable = self.setting.F[x],
                                             command = lambda x=x,s=self: s.cmd.do("scene F%d,store"%x))

            self.menuBar.addmenuitem('Recall', 'checkbutton', 'Recall F%d'%x,
                                             label='F%d'%x,
                                             variable = self.setting.F[x],
                                             command = lambda x=x,s=self: s.cmd.do("scene F%d"%x))

            self.menuBar.addmenuitem('Clear', 'checkbutton', 'F%d'%x,
                                     label='F%d'%x,
                                     variable = self.setting.F[x],
                                     command = lambda x=x,s=self: s.cmd.do("scene F%d,clear"%x))
            
#         self.menuBar.addmenuitem('ClearSHFT', 'checkbutton', 'SHFT-F%d'%x,
#                                  label='SHFT-F%d'%x,
#                                  variable = self.setting.SHFTF[x],
#                                  command = lambda x=x,s=self: s.cmd.do("scene SHFT-F%d,clear"%x))

        self.menuBar.addmenuitem('Scene', 'separator', '')
        
        self.menuBar.addmenuitem('Scene', 'checkbutton', 'Buttons',
                                 label='Buttons',
                                 variable = self.setting.scene_buttons,
                                 )

        self.menuBar.addcascademenu('Scene', 'Cache', 'Cache',
                                    label='Cache')
        
        self.menuBar.addmenuitem('Cache', 'command', 'Enable',
                                 label='Enable',
                                 command = lambda s=self:
                                 s.cmd.do("_ cache enable"))

        self.menuBar.addmenuitem('Cache', 'command', 'Optimize',
                                 label='Optimize',
                                 command = lambda s=self:
                                 s.cmd.do("_ cache optimize"))

        self.menuBar.addmenuitem('Cache', 'command', 'Read Only',
                                 label='Read Only',
                                 command = lambda s=self:
                                 s.cmd.do("_ cache read_only"))

        self.menuBar.addmenuitem('Cache', 'command', 'Disable',
                                 label='Disable',
                                 command = lambda s=self:
                                 s.cmd.do("_ cache disable"))

        self.menuBar.addmenu('Mouse', 'Mouse Configuration',tearoff=TRUE)

        self.menuBar.addcascademenu('Mouse', 'SelectionMode', 'Selection Mode',
                                             label='Selection Mode')

        self.menuBar.addmenuitem('SelectionMode', 'command', 'Atoms',
                                         label='Atoms',
                                         command = lambda s=self:
                                         s.cmd.do("_ set mouse_selection_mode,0,quiet=1"))

        self.menuBar.addmenuitem('SelectionMode', 'command', 'Residues',
                                         label='Residues',
                                         command = lambda s=self:
                                         s.cmd.do("_ set mouse_selection_mode,1,quiet=1"))

        self.menuBar.addmenuitem('SelectionMode', 'command', 'Chains',
                                         label='Chains',
                                         command = lambda s=self:
                                         s.cmd.do("_ set mouse_selection_mode,2,quiet=1"))

        self.menuBar.addmenuitem('SelectionMode', 'command', 'Segments',
                                         label='Segments',
                                         command = lambda s=self:
                                         s.cmd.do("_ set mouse_selection_mode,3,quiet=1"))

        self.menuBar.addmenuitem('SelectionMode', 'command', 'Objects',
                                         label='Objects',
                                         command = lambda s=self:
                                         s.cmd.do("_ set mouse_selection_mode,4,quiet=1"))

        self.menuBar.addmenuitem('SelectionMode', 'separator', '')

        self.menuBar.addmenuitem('SelectionMode', 'command', 'Molecules',
                                         label='Molecules',
                                         command = lambda s=self:
                                         s.cmd.do("_ set mouse_selection_mode,5,quiet=1"))

        self.menuBar.addmenuitem('SelectionMode', 'separator', '')

        self.menuBar.addmenuitem('SelectionMode', 'command', 'C-alphas',
                                         label='C-alphas',
                                         command = lambda s=self:
                                         s.cmd.do("_ set mouse_selection_mode,6,quiet=1"))

        self.menuBar.addmenuitem('Mouse', 'separator', '')

        self.menuBar.addmenuitem('Mouse', 'command', '3 Button Motions',
                                         label='3 Button Motions',
                                         command = lambda s=self: s.cmd.config_mouse('three_button_motions'))

        self.menuBar.addmenuitem('Mouse', 'command', '3 Button Editing',
                                         label='3 Button Editing',
                                         command = lambda s=self: s.cmd.config_mouse('three_button_editing'))

        self.menuBar.addmenuitem('Mouse', 'command', '3 Button Viewing',
                                         label='3 Button Viewing',
                                         command = lambda s=self: s.cmd.mouse('three_button_viewing'))

        self.menuBar.addmenuitem('Mouse', 'command', '3 Button Lights',
                                         label='3 Button Lights',
                                         command = lambda s=self: s.cmd.mouse('three_button_lights'))

        self.menuBar.addmenuitem('Mouse', 'command', '3 Button All Modes',
                                         label='3 Button All Modes',
                                         command = lambda s=self: s.cmd.config_mouse('three_button_all_modes'))

        self.menuBar.addmenuitem('Mouse', 'command', '2 Button Editing',
                                         label='2 Button Editing',
                                         command = lambda s=self: s.cmd.config_mouse('two_button_editing'))

        self.menuBar.addmenuitem('Mouse', 'command', '2 Button Viewing',
                                         label='2 Button Viewing',
                                         command = lambda s=self: s.cmd.config_mouse('two_button'))

        self.menuBar.addmenuitem('Mouse', 'command', '2 Button Lights',
                                         label='2 Button Lights',
                                         command = lambda s=self: s.cmd.config_mouse('two_button_lights'))

        self.menuBar.addmenuitem('Mouse', 'command', '1 Button Viewing Mode',
                                         label='1 Button Viewing Mode',
                                         command = lambda s=self: s.cmd.mouse('one_button_viewing'))

        self.menuBar.addmenuitem('Mouse', 'separator', '')

        self.menuBar.addcascademenu('Mouse', 'Emulate', 'Emulate',
                                             label='Emulate')

        self.menuBar.addmenuitem('Emulate', 'command', 'Maestro',
                                         label='Maestro',
                                         command = lambda s=self: s.cmd.mouse('three_button_maestro'))

        self.menuBar.addmenuitem('Mouse', 'separator', '')

        self.menuBar.addmenuitem('Mouse', 'checkbutton',
                                 'Virtual Trackball.',
                                 label='Virtual Trackball',
                                variable = self.setting.virtual_trackball,
                                )

        self.menuBar.addmenuitem('Mouse', 'checkbutton',
                                 'Show Mouse Grid.',
                                 label='Show Mouse Grid',
                                variable = self.setting.mouse_grid,
                                )

        self.menuBar.addmenuitem('Mouse', 'checkbutton',
                                 'Roving Origin.',
                                 label='Roving Origin',
                                variable = self.setting.roving_origin,
                                )

#        self.menuBar.addmenuitem('Mouse', 'checkbutton',
#                                 'Roving Detail.',
#                                 label='Roving Detail',
#                                variable = self.setting.roving_detail,
#                                )

        if sys.platform == 'darwin':
            self.menuBar.addmenuitem('Mouse', 'separator', '')

            self.menuBar.addcascademenu('Mouse', 'MacX11Focus', 'Mac OS X11',
                                        label='Mac OS X11')
            self.menuBar.addmenuitem('MacX11Focus', 'command',
                                     'Enable Click Through',
                                     label='Enable Click Through',
                                     command = lambda s=self: 
                                     s.toggleClickThrough(1))

            self.menuBar.addmenuitem('MacX11Focus', 'command',
                                     'Disable Click Through',
                                     label='Disable Click Through',
                                     command = lambda s=self: 
                                     s.toggleClickThrough(0))

        self.menuBar.addmenu('Wizard', 'Task Wizards',tearoff=TRUE)

        self.menuBar.addmenuitem('Wizard', 'command', 'Appearance',
                                         label='Appearance',
                                         command = lambda s=self: s.cmd.do("_ wizard appearance"))

        self.menuBar.addmenuitem('Wizard', 'command', 'Measurement',
                                         label='Measurement',
                                         command = lambda s=self: s.cmd.do("_ wizard measurement"))

        self.menuBar.addmenuitem('Wizard', 'command', 'Mutagenesis',
                                         label='Mutagenesis',
                                         command = lambda s=self: s.cmd.do("_ wizard mutagenesis"))

        self.menuBar.addmenuitem('Wizard', 'command', 'Pair Fitting',
                                         label='Pair Fitting',
                                         command = lambda s=self: s.cmd.do("_ wizard pair_fit"))

        self.menuBar.addmenuitem('Wizard', 'separator', '')
            
        self.menuBar.addmenuitem('Wizard', 'command', 'Density Map Wizard',
                                         label='Density',
                                         command = lambda s=self: s.cmd.do("_ wizard density"))

        self.menuBar.addmenuitem('Wizard', 'command', 'Filter',
                                         label='Filter',
                                         command = lambda s=self: s.cmd.do("_ wizard filter"))


        self.menuBar.addmenuitem('Wizard', 'command', 'Sculpting',
                                         label='Sculpting',
                                         command = lambda s=self: s.cmd.do("_ wizard sculpting"))

        if cleanup.auto_configure()>0:
            self.menuBar.addmenuitem('Wizard', 'separator', '')
        
            self.menuBar.addmenuitem('Wizard', 'command', 'Cleanup',
                                             label='Cleanup',
                                             command = lambda s=self: s.cmd.do("_ wizard cleanup"))

        self.menuBar.addmenuitem('Wizard', 'separator', '')
        
        self.menuBar.addmenuitem('Wizard', 'command', 'Label',
                                         label='Label',
                                         command = lambda s=self: s.cmd.do("_ wizard label"))

        self.menuBar.addmenuitem('Wizard', 'command', 'Charge',
                                         label='Charge',
                                         command = lambda s=self: s.cmd.do("_ wizard charge"))

        self.menuBar.addmenuitem('Wizard', 'separator', '')
        
        self.menuBar.addcascademenu('Wizard', 'Demo', 'Demo',
                                             label='Demo',tearoff=TRUE)

        self.menuBar.addmenuitem('Demo', 'command', 'Representations',
                                         label='Representations',
                                         command = lambda s=self: s.cmd.do(
            "_ replace_wizard demo,reps"))

        self.menuBar.addmenuitem('Demo', 'command', 'Cartoon Ribbons',
                                         label='Cartoon Ribbons',
                                         command = lambda s=self: s.cmd.do(
            "_ replace_wizard demo,cartoon"))

        self.menuBar.addmenuitem('Demo', 'command', 'Roving Detail',
                                         label='Roving Detail',
                                         command = lambda s=self: s.cmd.do(
            "_ replace_wizard demo,roving"))
        
        self.menuBar.addmenuitem('Demo', 'command', 'Roving Density',
                                         label='Roving Density',
                                         command = lambda s=self: s.cmd.do(
            "_ replace_wizard demo,roving_density"))

        self.menuBar.addmenuitem('Demo', 'command', 'Transparency',
                                         label='Transparency',
                                         command = lambda s=self: s.cmd.do(
            "_ replace_wizard demo,trans"))

        self.menuBar.addmenuitem('Demo', 'command', 'Ray Tracing',
                                         label='Ray Tracing',
                                         command = lambda s=self: s.cmd.do(
            "_ replace_wizard demo,ray"))

        self.menuBar.addmenuitem('Demo', 'command', 'Sculpting',
                                         label='Sculpting',
                                         command = lambda s=self: s.cmd.do(
            "_ replace_wizard demo,sculpt"))

        self.menuBar.addmenuitem('Demo', 'command', 'Scripted Animation',
                                         label='Scripted Animation',
                                         command = lambda s=self: s.cmd.do(

            "_ replace_wizard demo,anime"))


        self.menuBar.addmenuitem('Demo', 'command', 'Electrostatics',
                                         label='Electrostatics',
                                         command = lambda s=self: s.cmd.do(
            "_ replace_wizard demo,elec"))


        self.menuBar.addmenuitem('Demo', 'command', 'Compiled Graphics Objects',
                                         label='Compiled Graphics Objects',
                                         command = lambda s=self: s.cmd.do(
            "_ replace_wizard demo,cgo"))

        self.menuBar.addmenuitem('Demo', 'command', 'MolScript/Raster3D Input',
                                         label='Molscript/Raster3D Input',
                                         command = lambda s=self: s.cmd.do(
            "_ replace_wizard demo,raster3d"))

        self.menuBar.addmenuitem('Demo', 'separator', '')
        
        self.menuBar.addmenuitem('Demo', 'command', 'End Demonstration',
                                         label='End Demonstration',
                                         command = lambda s=self: s.cmd.do(
            '_ replace_wizard demo,finish'))

        self.menuBar.addmenu('Plugin', 'Plugin',tearoff=TRUE)      

    def show_about(self):
        Pmw.aboutversion(self.appversion)
        Pmw.aboutcopyright(self.copyright)
        Pmw.aboutcontact(
             'For more information, browse to: %s\n or send email to: %s' %\
             (self.contactweb, self.contactemail))
        self.about = Pmw.AboutDialog(self.root, applicationname=self.appname)
        self.my_activate(self.about)
        self.about.withdraw()
        
    def createInterface(self):

        self.balloon = Pmw.Balloon(self.root)

        self.createMenuBar()

        self.app.menuBar = self.menuBar # to support legacy plugins    
        
        self.app.initializePlugins()
        
        self.createDataArea()

        self.createCommandArea()

        self.createButtons()

        self.createMessageBar()

        self.createConsole()

    def setup(self):

        # call the parent method
        PMGSkin.setup(self)
        
        # name the application
        self.root.title(self.appname)

        # create the user interface
        self.createInterface()

        # pack the root window
        self.app._hull.pack(side=LEFT, fill=BOTH, expand=YES, anchor=CENTER)

        # and set focus
        if hasattr(self,'entry'): self.entry.focus_set()

    def takedown(self):
        self.destroyMessageBar()
        self.destroyDataArea()
        self.destroyCommandArea()
        self.destroyButtonArea()
        self.balloon.destroy()
        self.menuBar.destroy()
        
    def __init__(self,app):

        PMGSkin.__init__(self,app)
        Normal.appversion = app.pymol.cmd.get_version()[0]
        self.app = app
        self.save_file = ''
        self.cmd = app.pymol.cmd
        self.util = app.pymol.util
        self.movie_command = None
        self.movie_start = 1
        self.auto_overlay = None
        self.edit_mode = None
        self.valence = None
        self._initialdir = ''

def __init__(app):
    return Normal(app)

    
