#A* -------------------------------------------------------------------
#B* This file contains source code for the PyMOL computer program
#C* copyright 1998-2000 by Warren Lyford Delano of DeLano Scientific. 
#D* -------------------------------------------------------------------
#E* It is unlawful to modify or remove this copyright notice.
#F* -------------------------------------------------------------------
#G* Please see the accompanying LICENSE file for further information. 
#H* -------------------------------------------------------------------
#I* Additional authors of this source file include:
#-* 
#-* 
#-*
#Z* -------------------------------------------------------------------

if __name__=='pymol.wizarding':

    import pymol
    import imp
    import sys
    import string
    import cmd
    from cmd import _cmd,lock,unlock,Shortcut,QuietException,_raising, \
          _feedback,fb_module,fb_mask, \
          DEFAULT_ERROR, DEFAULT_SUCCESS, _raising, is_ok, is_error
    
    import cPickle
    import traceback
    
    def _wizard(name,arg,kwd,replace):
        r = DEFAULT_ERROR
        import wizard
        try:
            full_name = 'pymol.wizard.'+name
            if not sys.modules.has_key(full_name):
                mod_tup = imp.find_module(name,wizard.__path__)
                mod_obj = imp.load_module(full_name,mod_tup[0],
                                                  mod_tup[1],mod_tup[2])
            else:
                mod_obj = sys.modules[full_name]
            if mod_obj:
                oname = string.capitalize(name)
                r = DEFAULT_SUCCESS
                if hasattr(mod_obj,oname):
                    wiz = apply(getattr(mod_obj,oname),arg,kwd)
                    if wiz:
                        set_wizard(wiz,replace)
                        cmd.do("_ refresh_wizard")
                else:
                    print "Error: Sorry, couldn't find the '"+oname+"' class."                             
            else:
                print "Error: Sorry, couldn't import the '"+name+"' wizard."         
        except ImportError:
            print "Error: Sorry, couldn't import the '"+name+"' wizard."         
        return r
    
    def wizard(name=None,*arg,**kwd):
        '''
DESCRIPTION

    "wizard" launches on of the built-in wizards.  There are special
    Python scripts which work with PyMOL in order to obtain direct user
    interaction and easily peform complicated tasks.

USAGE

    wizard name

PYMOL API

    cmd.wizard(string name)

EXAMPLE

    wizard distance  # launches the distance measurement wizard
    '''
        r = DEFAULT_ERROR
        if name==None:
            cmd.set_wizard()
            r = DEFAULT_SUCCESS
        else:
            name = str(name)
            if string.lower(name)=='distance': # legacy compatibility
                name = 'measurement'
            r = _wizard(name,arg,kwd,0)
        if _raising(r): raise pymol.CmdException
        return r
        
    def replace_wizard(name=None,*arg,**kwd):
        r = DEFAULT_ERROR
        if name==None:
            cmd.set_wizard()
            r = DEFAULT_SUCCESS
        else:
            r = _wizard(name,arg,kwd,1)
        if _raising(r): raise pymol.CmdException
        return r

    def set_wizard(wizard=None,replace=0): # INTERNAL
        r = DEFAULT_ERROR
        try:
            lock()
            r = _cmd.set_wizard(wizard,replace)
        finally:
            unlock(r)
        if _raising(r): raise pymol.CmdException
        return r

    def set_wizard_stack(stack=[]): # INTERNAL
        r = DEFAULT_ERROR
        try:
            lock()
            r = _cmd.set_wizard_stack(stack)
        finally:
            unlock(r)
        if _raising(r): raise pymol.CmdException
        return r

    def refresh_wizard(): # INTERNAL
        r = DEFAULT_ERROR      
        try:
            lock()
            r = _cmd.refresh_wizard()
        finally:
            unlock(r)
        if _raising(r): raise pymol.CmdException
        return r

    def dirty_wizard(): # INTERNAL
        r = DEFAULT_ERROR
        try:
            lock()
            r = _cmd.dirty_wizard()
        finally:
            unlock(r)
        if _raising(r): raise pymol.CmdException
        return r

    def get_wizard(): # INTERNAL
        r = DEFAULT_ERROR
        try:
            lock()
            r = _cmd.get_wizard()
        finally:
            unlock(r)
        if _raising(r): raise pymol.CmdException
        return r

    def get_wizard_stack(): # INTERNAL
        r = DEFAULT_ERROR
        try:
            lock()
            r = _cmd.get_wizard_stack()
        finally:
            unlock(r)
        if _raising(r): raise pymol.CmdException
        return r

    def session_save_wizard(session):
        # double-pickle so that session file is class-independent
        session['wizard']=cPickle.dumps(cmd.get_wizard_stack(),1)
        return 1

    def session_restore_wizard(session):
        if session!=None:
            if session.has_key('wizard'):
                from pymol.wizard import message
                import __main__
#         __main__.message = message
                sys.modules['message'] = message
                try:
                    wizards = cPickle.loads(session['wizard'])
                    cmd.set_wizard_stack(wizards)
                except:
                    print "Session-Warning: unable to restore wizard."
        return 1

    if session_restore_wizard not in pymol._session_restore_tasks:
        pymol._session_restore_tasks.append(session_restore_wizard)

    if session_save_wizard not in pymol._session_save_tasks:
        pymol._session_save_tasks.append(session_save_wizard)

