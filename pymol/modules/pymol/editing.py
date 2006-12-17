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

if __name__=='pymol.editing':

    import pymol
    import math
    import selector
    import cmd
    from cmd import _cmd,lock,unlock,Shortcut,is_string, \
          boolean_sc,boolean_dict,safe_list_eval, is_sequence, \
          DEFAULT_ERROR, DEFAULT_SUCCESS, _raising, is_ok, is_error              
    from chempy import cpv

    def sculpt_purge():
        '''
DESCRIPTION

    "sculpt_purge" is not officially supported in PyMOL 1.x.
    
    '''
        r = DEFAULT_ERROR
        try:
            lock()
            r = _cmd.sculpt_purge()
        finally:
            unlock(r)
        if _raising(r): raise pymol.CmdException            
        return r

    def sculpt_deactivate(object):
        '''
DESCRIPTION

    "sculpt_deactivate" is not officially supported in PyMOL 1.x.

    '''
        r = 0
        try:
            lock()
            r = _cmd.sculpt_deactivate(str(object))
        finally:
            unlock(r)
        if _raising(r): raise pymol.CmdException            
        return r

    def sculpt_activate(object,state=0,match_state=-1,match_by_segment=0):
        '''
DESCRIPTION

    "sculpt_deactivate" is not officially supported in PyMOL 1.x.

    '''
        r = DEFAULT_ERROR
        try:
            lock()
            r = _cmd.sculpt_activate(str(object),int(state)-1,int(match_state)-1,int(match_by_segment))
        finally:
            unlock(r)
        if _raising(r): raise pymol.CmdException            
        return r

    def split_states(object,first=1,last=0,prefix=None):
        '''
DESCRIPTION

    "split_states" separates a multi-state molecular object into a set
    of single-state molecular objects.

USAGE

    split_states object [, first [,last [, prefix ]]]
    
EXAMPLE

    load docking_hits.sdf
    split_states docking_hits, prefix=hit
    delete docking_hits
    
        '''
        r = DEFAULT_SUCCESS
        object = str(object)
        if prefix==None:
            prefix = object+"_"
        first=int(first)
        last=int(last)
        n_state = cmd.count_states(object)
        if n_state<0:
            r = DEFAULT_ERROR
        else:
            if last<1:
                last = n_state
            for a in range(first,last+1):
                try:
                    name = cmd.get_title(object,a)
                    if len(name)==0:
                        name = prefix+"%04d"%a
                except:
                    name = prefix+"%04d"%a
                r = cmd.frame(a)
                if is_error(r): 
                    break
                r = cmd.create(name,"%s and present"%object,a,1)
                if is_error(r): 
                    break
        if _raising(r): raise pymol.CmdException            
        return r
    
    def sculpt_iterate(object,state=0,cycles=10):
        '''
        
    "sculpt_iterate" is not officially supported in PyMOL 1.x.
    
    '''
        r = DEFAULT_ERROR
        try:
            lock()
            r = _cmd.sculpt_iterate(str(object),int(state)-1,int(cycles))
        finally:
            unlock(r)
        if _raising(r): raise pymol.CmdException            
        return r

    def smooth(selection="all",passes=1,window=5,first=1,last=0,ends=0,quiet=1):
        '''
DESCRIPTION

    "smooth" performs a window average over a series of states.  This
    type of averaging is often used to suppress high-frequency vibrations
    in a molecular dynamics trajectory.

USAGE

    smooth [ selection [, passes [,window [,first [,last [, ends]]]]]]

SEE ALSO

    load_traj

ARGUMENTS

    ends = 0 or 1: controls whether or not the end states are also smoothed
    using a weighted asymmetric window

NOTES

    This function is not memory efficient.  For reasons of
    flexibility, it uses two additional copies of every atomic
    coordinate for the calculation.  If you are memory-constrained in
    visualizing MD trajectories, then you may want to use an external
    tool such as ptraj to perform smoothing before loading coordinates
    into PyMOL.
    '''
        
        r = DEFAULT_ERROR
        selection = selector.process(selection)   
        try:
            lock()
            r = _cmd.smooth(str(selection),int(passes),int(window),
                                 int(first)-1,int(last)-1,int(ends),int(quiet))

        finally:
            unlock(r)
        if _raising(r): raise pymol.CmdException            
        return r

    def set_symmetry(selection,a,b,c,alpha,beta,gamma,spacegroup="P1"):
        '''
DESCRIPTION

    "set_symmetry" can be used to define or redefine the crystal
    and spacegroup parameters for a molecule or map object.

USAGE

    set_symmetry selection, a, b, c, alpha, beta, gamma, spacegroup

PYMOL API

    cmd.set_symmetry(string selection, float a, float b, float c,
          float alpha, float beta, float gamma, string spacegroup)

NOTES

    The new symmetry will be defined for every object referenced
    by the selection.
        '''
        r = DEFAULT_ERROR
        selection = selector.process(selection)
        try:
            lock()
            r = _cmd.set_symmetry(str(selection),
                                         float(a),float(b),float(c),
                                         float(alpha),float(beta),float(gamma),
                                         str(spacegroup))
        finally:
            unlock(r)
        if _raising(r): raise pymol.CmdException            
        return r

    def set_name(old_name, new_name):
        '''
DESCRIPTION

    "set_name" can be used to change the name of an object or selection
    
USAGE

    set_name old_name, new_name
    
PYMOL API

    cmd.set_name(string old_name, string new_name)

        '''
        r = DEFAULT_ERROR
        try:
            lock()
            r = _cmd.set_name(str(old_name),
                                    str(new_name))
        finally:
            unlock(r)
        if _raising(r): raise pymol.CmdException            
        return r


    def set_geometry(selection,geometry,valence):
        '''
DESCRIPTION

    "set_geometry" changes PyMOL\'s assumptions about the proper valence
    and geometry of the picked atom.

USAGE

    set_geometry geometry, valence

PYMOL API

    cmd.set_geometry(int geometry,int valence )

NOTES

    Immature functionality. See code for details.

SEE ALSO

    remove, attach, fuse, bond, unbond
    '''
        r = DEFAULT_ERROR
        # preprocess selection
        selection = selector.process(selection)
        try:
            lock()
            r = _cmd.set_geometry(str(selection),int(geometry),int(valence))
        finally:
            unlock(r)
        if _raising(r): raise pymol.CmdException            
        return r

    def undo():
        '''
DESCRIPTION

    "undo" restores the previous conformation of the object currently
    being edited.

USAGE

    undo

SEE ALSO

    redo, push_undo
    '''
        r = DEFAULT_ERROR      
        try:
            lock()
            r = _cmd.undo(-1)
        finally:
            unlock(r)
        if _raising(r): raise pymol.CmdException            
        return r

    def push_undo(selection,state=0):
        '''
DESCRIPTION

    "push_undo" stores the currently conformations of objects in the
    selection onto their individual kill rings.

USAGE

    push_undo (all)

SEE ALSO

    undo, redo
    '''
        # preprocess selections
        selection = selector.process(selection)
        #
        r = DEFAULT_ERROR
        try:
            lock()
            r = _cmd.push_undo("("+str(selection)+")",int(state)-1)
        finally:
            unlock(r)
        if _raising(r): raise pymol.CmdException            
        return r

    def redo():
        '''
DESCRIPTION

    "redo" reapplies the conformational change of the object currently
    being edited.

USAGE

    redo

SEE ALSO

    undo, push_undo
    '''
        r = DEFAULT_ERROR      
        try:
            lock()
            r = _cmd.undo(1)
        finally:
            unlock(r)
        if _raising(r): raise pymol.CmdException                     
        return r

    def bond(atom1="(pk1)",atom2="(pk2)",order=1,edit=1):
        '''
DESCRIPTION

    "bond" creates a new bond between two selections, each of
    which should contain one atom.

USAGE

    bond [atom1, atom2 [,order]]

PYMOL API

    cmd.bond(string atom1, string atom2)

NOTES

    The atoms must both be within the same object.

    The default behavior is to create a bond between the (lb) and (rb)
    selections.

SEE ALSO

    unbond, fuse, attach, replace, remove_picked
    '''
        r = DEFAULT_ERROR
        # preprocess selections
        atom1 = selector.process(atom1)
        atom2 = selector.process(atom2)
        try:
            lock()
            r = _cmd.bond(atom1,atom2,int(order),1)
        finally:
            unlock(r)
        if _raising(r): raise pymol.CmdException            
        return r

    def invert(quiet=1):
        '''
DESCRIPTION

    "invert" inverts the stereo-chemistry of atom (pk1), holding attached atoms
    (pk2) and (pk3) immobile.

USAGE

    invert 

PYMOL API

    cmd.invert( )

NOTE

    The invert function is usually bound to CTRL-E in Editing Mode.


    '''
        r = DEFAULT_ERROR
        # preprocess selections
        #
        try:
            lock()
            r = _cmd.invert(int(quiet))
        finally:
            unlock(r)
        if _raising(r): raise pymol.CmdException            
        return r

    def unbond(atom1="(pk1)",atom2="(pk2)"):
        '''
DESCRIPTION

    "unbond" removes all bonds between two selections.

USAGE

    unbond atom1,atom2

PYMOL API

    cmd.unbond(selection atom1="(pk1)",selection atom2="(pk2)")

SEE ALSO

    bond, fuse, remove_picked, attach, detach, replace

    '''
        r = DEFAULT_ERROR
        # preprocess selections
        atom1 = selector.process(atom1)
        atom2 = selector.process(atom2)   
        try:
            lock()
            r = _cmd.bond(str(atom1),str(atom2),0,0)
        finally:
            unlock(r)
        if _raising(r): raise pymol.CmdException            
        return r

    def remove(selection,quiet=1):
        '''
DESCRIPTION

    "remove" eleminates a selection of atoms from models.

USAGE

    remove (selection)

PYMOL API

    cmd.remove( string selection )

EXAMPLES

    remove ( resi 124 )

SEE ALSO

    delete
    '''
        r = DEFAULT_ERROR
        # preprocess selection
        selection = selector.process(selection)
        #   
        r = 1
        try:
            lock()   
            r = _cmd.remove("("+selection+")",int(quiet))
        finally:
            unlock(r)
        if _raising(r): raise pymol.CmdException            
        return r


    def remove_picked(hydrogens=1,quiet=1):
        '''
DESCRIPTION

    "remove_picked" removes the atom or bond currently
    picked for editing. 

USAGE

    remove_picked [hydrogens]

PYMOL API

    cmd.remove_picked(integer hydrogens=1)

NOTES

    This function is usually connected to the
    DELETE key and "CTRL-D".

    By default, attached hydrogens will also be deleted unless
    hydrogen-flag is zero.

SEE ALSO

    attach, replace
    '''
        r = DEFAULT_ERROR
        try:
            lock()   
            r = _cmd.remove_picked(int(hydrogens),int(quiet))
        finally:
            unlock(r)
        if _raising(r): raise pymol.CmdException            
        return r


    def cycle_valence(h_fill=1,quiet=1):
        '''
DESCRIPTION

    "cycle_valence" cycles the valence on the currently selected bond.

USAGE

    cycle_valence [ h_fill ]

PYMOL API

    cmd.cycle_valence(int h_fill)

EXAMPLES

    cycle_valence
    cycle_valence 0

NOTES

    If the h_fill flag is true, hydrogens will be added or removed to
    satisfy valence requirements.

    This function is usually connected to the DELETE key and "CTRL-W".

SEE ALSO

    remove_picked, attach, replace, fuse, h_fill
    '''
        r = DEFAULT_ERROR
        try:
            lock()   
            r = _cmd.cycle_valence(quiet)
        finally:
            unlock(r)
        if h_fill:
            globals()['h_fill'](quiet)
        if _raising(r): raise pymol.CmdException            
        return r


    def attach(element,geometry,valence,name='',quiet=1):
        '''
DESCRIPTION

    "attach" adds a single atom onto the picked atom.

USAGE

    attach element, geometry, valence

PYMOL API

    cmd.attach( element, geometry, valence )

NOTES

    Immature functionality.  See code for details.

    '''
        r = DEFAULT_ERROR
        try:
            lock()   
            r = _cmd.attach(str(element),int(geometry),int(valence),str(name))
        finally:
            unlock(r)
        if _raising(r): raise pymol.CmdException            
        return r


    def fuse(selection1="(pk1)",selection2="(pk2)",mode=0,recolor=1,move=1):
        '''
DESCRIPTION

    "fuse" joins two objects into one by forming a bond.  A copy of
    the object containing the first atom is moved so as to form an
    approximately resonable bond with the second, and is then merged
    with the first object.

USAGE

    fuse (selection1), (selection2)

PYMOL API

    cmd.fuse( string selection1="(pk1)", string selection2="(pk2)" )

NOTES

    Each selection must include a single atom in each object.
    The atoms can both be hydrogens, in which case they are
    eliminated, or they can both be non-hydrogens, in which
    case a bond is formed between the two atoms.

SEE ALSO

    bond, unbond, attach, replace, fuse, remove_picked
    '''
        r = DEFAULT_ERROR
        # preprocess selections
        selection1 = selector.process(selection1)
        selection2 = selector.process(selection2)
        #   
        try:
            lock()
            r = _cmd.fuse(str(selection1),str(selection2),
                              int(mode),int(recolor),int(move))
        finally:
            unlock(r)
        if _raising(r): raise pymol.CmdException            
        return r

    def unpick(*arg):
        '''
DESCRIPTION

    "unpick" deletes the special "pk" atom selections (pk1, pk2, etc.)
    used in atom picking and molecular editing.

USAGE

    unpick

PYMOL API

    cmd.unpick()

SEE ALSO

    edit
        '''
        
        r = DEFAULT_ERROR      
        try:
            lock()   
            r = _cmd.unpick()
        finally:
            unlock(r)
        if _raising(r): raise pymol.CmdException            
        return r

    def drag(selection=None, wizard=1, edit=1, quiet=1):
        '''
        '''
        quiet = int(quiet)
        if (selection!=None) and (selection!=""):
            selection = selector.process(selection)
            if is_string(edit):
                edit=boolean_dict[boolean_sc.auto_err(edit,'boolean')]
            if is_string(wizard):
                wizard=boolean_dict[boolean_sc.auto_err(wizard,'boolean')]
            edit = int(edit)
            wizard = int(wizard)
            old_button_mode = cmd.get('button_mode')
        else:
            wizard = 0
            edit = 0
            selection = ""
        #
        r = DEFAULT_ERROR
        try:
            lock()   
            r = _cmd.drag(str(selection),int(quiet))
        finally:
            unlock(r)
        if not is_error(r):
            if edit:
                cmd.edit_mode(edit)
            if wizard:
                wiz = cmd.get_wizard()
                if (wiz == None):
                    cmd.wizard("dragging",old_button_mode)
                elif wiz.__class__ != 'pymol.wizard.dragging.Dragging':
                    cmd.wizard("dragging",old_button_mode)
                else:
                    wiz.recount()
        if _raising(r): raise pymol.CmdException
        return r
        
    def edit(selection1='',selection2='none',selection3='none',
                selection4='none',pkresi=0, pkbond=1, quiet=1):
        '''
DESCRIPTION

    "edit" picks an atom or bond for editing.

USAGE

    edit (selection) [ ,(selection) ]

PYMOL API

    cmd.edit( string selection  [ ,string selection ] )

NOTES

    If only one selection is provided, an atom is picked.
    If two selections are provided, the bond between them
    is picked (if one exists).

SEE ALSO

    unpick, remove_picked, cycle_valence, torsion
    '''
        # preprocess selections
        selection1 = selector.process(selection1)
        selection2 = selector.process(selection2)
        selection3 = selector.process(selection3)
        selection4 = selector.process(selection4)
        #
        r = DEFAULT_ERROR
        try:
            lock()   
            r = _cmd.edit(str(selection1),str(selection2),
                              str(selection3),str(selection4),
                              int(pkresi),int(pkbond),int(quiet))
        finally:
            unlock(r)
        if _raising(r): raise pymol.CmdException
        return r

    def get_editor_scheme():
        r = DEFAULT_ERROR
        try:
            lock()   
            r = _cmd.get_editor_scheme()
        finally:
            unlock(r)
        if _raising(r): raise pymol.CmdException            
        return r

    def torsion(angle):
        '''
DESCRIPTION

    "torsion" rotates the torsion on the bond currently
    picked for editing.  The rotated fragment will correspond
    to the first atom specified when picking the bond (or the
    nearest atom, if picked using the mouse).

USAGE

    torsion angle

PYMOL API

    cmd.torsion( float angle )

SEE ALSO

    edit, unpick, remove_picked, cycle_valence
    '''
        r = DEFAULT_ERROR
        try:
            lock()   
            r = _cmd.torsion(float(angle))
        finally:
            unlock(r)
        if _raising(r): raise pymol.CmdException            
        return r

    def h_fill(quiet=1):
        '''
DESCRIPTION

    "h_fill" removes and replaces hydrogens on the atom
    or bond picked for editing.  

USAGE

    h_fill

PYMOL API

    cmd.h_fill()

NOTES

    This is useful for fixing hydrogens after changing
    bond valences.

SEE ALSO

    edit, cycle_valence, h_add
    '''
        r = DEFAULT_ERROR
        try:
            lock()   
            r = _cmd.h_fill(int(quiet))
        finally:
            unlock(r)
        if _raising(r): raise pymol.CmdException            
        return r

    def h_fix(selection="",quiet=1):
        '''
under development...
    '''
        # preprocess selection
        selection = selector.process(selection)

        r = DEFAULT_ERROR
        try:
            lock()   
            r = _cmd.h_fix(str(selection),int(quiet))
        finally:
            unlock(r)
        if _raising(r): raise pymol.CmdException            
        return r


    def h_add(selection="(all)",quiet=1):
        '''
DESCRIPTION

    "h_add" uses a primitive algorithm to add hydrogens
    onto a molecule.

USAGE

    h_add (selection)

PYMOL API

    cmd.h_add( string selection="(all)" )

SEE ALSO

    h_fill
    '''
        # preprocess selection
        selection = selector.process(selection)
        #   
        r = DEFAULT_ERROR
        try:
            lock()   
            r = _cmd.h_add(selection,int(quiet))
        finally:
            unlock(r)
        if _raising(r): raise pymol.CmdException            
        return r



    def sort(object=""):
        '''
DESCRIPTION

    "sort" reorders atoms in the structure.  It usually only necessary
    to run this routine after an "alter" command which has modified the
    names of atom properties.  Without an argument, sort will resort
    all atoms in all objects.

USAGE

    sort [object]

PYMOL API

    cmd.sort(string object)

SEE ALSO

    alter
    '''
        r = DEFAULT_ERROR
        try:
            lock()
            r = _cmd.sort(str(object))
        finally:
            unlock(r)
        if _raising(r): raise pymol.CmdException            
        return r


    def replace(element, geometry, valence,
                h_fill=1, name="", quiet=1):
        '''
DESCRIPTION

    "replace" replaces the picked atom with a new atom.

USAGE

    replace element, geometry, valence [,h_fill [,name ]]

PYMOL API

    cmd.replace(string element, int geometry, int valence,
                    int h_fill = 1, string name = "" )

NOTES

    Immature functionality. See code for details.

SEE ALSO

    remove, attach, fuse, bond, unbond
    '''
        r = DEFAULT_ERROR      
        if not "pk1" in cmd.get_names("selections"):
            print " Error: you must first pick an atom to replace."
            raise pymol.CmdException
        try:
            if h_fill: # strip off existing hydrogens
                remove("((neighbor pk1) and elem h)",quiet=quiet)
            lock()
            r = _cmd.replace(str(element),int(geometry),int(valence),str(name),quiet)
        finally:
            unlock(r)
        if _raising(r): raise pymol.CmdException            
        return r

    def rename(object,force=0):
        '''
DESCRIPTION

    "rename" creates new atom names which are unique within residues.

USAGE

    CURRENT
        rename object-name [ ,force ]

        force = 0 or 1 (default: 0)

    PROPOSED
        rename object-or-selection,force   

PYMOL API

    CURRENT
        cmd.rename( string object-name, int force )

    PROPOSED
        cmd.rename( string object-or-selection, int force )

NOTES

    To regerate only some atom names in a molecule, first clear them
    with an "alter (sele),name=''" commmand, then use "rename"

SEE ALSO

    alter
    '''
        r = DEFAULT_ERROR   
        try:
            lock()   
            r = _cmd.rename(str(object),int(force))
        finally:
            unlock(r)
        if _raising(r): raise pymol.CmdException            
        return r


    def dss(selection="(all)", state=0, context=None,
            preserve=0, quiet=1):
        '''
DESCRIPTION

    "dss" defines secondary structure based on backbone geometry
    and hydrogen bonding patterns.

    With PyMOL, heavy emphasis is placed on cartoon aesthetics, and so
    both hydrogen bonding patterns and backbone geometry are used in
    the assignment process.  Depending upon the local context, helix
    and strand assignments are made based on geometry, hydrogen
    bonding, or both.

    This command will generate results which differ slightly from DSSP
    and other programs.  Most deviations occur in borderline or
    transition regions.  Generally speaking, PyMOL is more strict, thus
    assigning fewer helix/sheet residues, except for partially
    distorted helices, which PyMOL tends to tolerate.
    
    WARNING: This algorithm has not yet been rigorously validated.
    
USAGE

    dss selection, state

    state = state-index or 0 for all states
    
EXAMPLES

    dss

NOTES

    If you dislike one or more of the assignments made by dss, you can
    use the alter command to make changes (followed by "rebuild").
    For example:
    
        alter 123-125/, ss=\'L\'
        alter pk1, ss=\'S\'
        alter 90/, ss=\'H\'
        rebuild
        
        '''
        # preprocess selections
        selection = selector.process(selection)
        r = DEFAULT_ERROR
        if context==None:
            context = ""
        else:
            context = selector.process(context)
        #
        try:
            lock()
            r = _cmd.dss(str(selection),int(state)-1,str(context),
                            int(preserve),int(quiet))
        finally:
            unlock(r)   
        if _raising(r): raise pymol.CmdException            
        return r

    def alter(selection, expression, quiet=1,
              space=cmd.pymol.__dict__):
        '''
DESCRIPTION

    "alter" changes atomic properties using an expression evaluated
    within a temporary namespace for each atom.

USAGE

    alter selection, expression

EXAMPLES

    alter chain A, chain='B'
    alter all, resi=str(int(resi)+100)
    sort

NOTES

    Symbols defined (* = read only):

    name, resn, resi, resv, chain, segi, elem, alt, q, b, vdw, type,
    partial_charge, formal_charge, elec_radius, text_type, label, 
    numeric_type, model*, state*, index*, ID, rank, color, ss,
    cartoon, flags

    All strings must be explicitly quoted.  This operation typically
    takes several seconds per thousand atom altered.  

    You may need to issue a "rebuild" in order to update associated
    representations.
    
    WARNING: You should always issue a "sort" command on an object
    after modifying any property which might affect canonical atom
    ordering (names, chains, etc.).  Failure to do so will confound
    subsequent "create" and "byres" operations.  

SEE ALSO

    alter_state, iterate, iterate_state, sort
        '''
        r = DEFAULT_ERROR
        # preprocess selections
        selection = selector.process(selection)
        #
        try:
            lock()
            r = _cmd.alter("("+str(selection)+")",str(expression),0,int(quiet),dict(space))
        finally:
            unlock(r)   
        if _raising(r): raise Qui
        return r

    def alter_list(object, expr_list, quiet=1, space=cmd.pymol.__dict__):
        '''
DESCRIPTION

    "alter_list" is not complete or supported in PyMOL 1.x.
    
        '''
        #
        try:
            lock()
            r = _cmd.alter_list(str(object),list(expr_list),int(quiet),dict(space))
        finally:
            unlock(r)   
        if _raising(r): raise pymol.CmdException            
        return r


    def iterate(selection,expression,quiet=1,space=cmd.pymol.__dict__):
        '''
DESCRIPTION

    "iterate" iterates over an expression within a temporary namespace
    for each atom.

USAGE

    iterate (selection),expression

EXAMPLES

    stored.net_charge = 0
    iterate all, stored.net_charge = stored.net_charge + partial_charge
    print stored.net_charge
    
    stored.names = []
    iterate all, stored.names.append(name)
    print stored.names
    
NOTES

    Unlike with the "alter" comman3d, atomic properties cannot be
    altered.  For this reason, "iterate" is more efficient than
    "alter".

SEE ALSO

    iterate_state, alter, alter_state
        '''
        r = DEFAULT_ERROR
        # preprocess selection
        selection = selector.process(selection)
        #
        try:
            lock()
            r = _cmd.alter("("+str(selection)+")",str(expression),1,int(quiet),dict(space))
        finally:
            unlock(r)   
        if _raising(r): raise pymol.CmdException            
        return r

    def alter_state(state, selection, expression, quiet=1,
                    space=cmd.pymol.__dict__, atomic=1):
        '''
DESCRIPTION

    "alter_state" changes atom coordinates and flags over a particular
    state and selection using the Python evaluator with a temporary
    namespace for each atomic coordinate.

USAGE

    alter_state state, selection, expression

EXAMPLES

    alter_state 1, (all), x=x+5
    rebuild
    
NOTES

    By default, most of the symbols from "alter" are available for use
    on a read-only basis.  It is usually necessary to "rebuild"
    representations once your alterations are completed.
    
SEE ALSO

    iterate_state, alter, iterate
        '''
        
        r = DEFAULT_ERROR
        # preprocess selection
        selection = selector.process(selection)
        #
        state = int(state)
        try:
            lock()
            r = _cmd.alter_state(int(state)-1,"("+str(selection)+")",str(expression),
                                        0,int(atomic),int(quiet),dict(space))
        finally:
            unlock(r)   
        if _raising(r): raise pymol.CmdException            
        return r

    def iterate_state(state,selection,expression,quiet=1,space=cmd.pymol.__dict__,atomic=1):
        '''
DESCRIPTION

    "iterate_state" is to "alter_state" as "iterate" is to "alter"

USAGE

    iterate_state state, selection, expression

EXAMPLES

    stored.sum_x = 0.0
    iterate_state 1, all, stored.sum_x = stored.sum_x + x
    print stored.sum_x
    
SEE ALSO

    iterate, alter, alter_state
        '''
        # preprocess selection
        selection = selector.process(selection)
        state = int(state)
        #
        try:
            lock()
            r = _cmd.alter_state(int(state)-1,"("+str(selection)+")",
                                        str(expression),1,int(atomic),
                                        int(quiet),dict(space))
        finally:
            unlock(r)   
        if _raising(r): raise pymol.CmdException            
        return r

    def translate(vector=[0.0,0.0,0.0],selection="all",
                      state=0,camera=1,object=None,object_mode=0):
        '''
DESCRIPTION

    "translate" can be used to translate the atomic coordinates of a
    molecular object.  Behavior differs depending on whether or not the
    "object" parameter is specified.

    If object is None, then translate translates atomic coordinates
    according to the vector provided for the selection and in the state
    provided.  All representation geometries will need to be
    regenerated to reflect the new atomic coordinates.

    If object is set to an object name, then selection and state are
    ignored and instead of translating the atomic coordinates, the
    object\'s overall representation display matrix is modified.  This
    option is for use in animations only.

    The "camera" option controls whether the camera or the model\'s
    axes are used to interpret the translation vector.

USAGE

    translate vector [,selection [,state [,camera [,object ]]]]

PYMOL API

    cmd.translate(list vector, string selection = "all", int state = 0,
                      int camera = 1, string object = None)

EXAMPLES

    translate [1,0,0], name ca

NOTES

    if state = 0, then only visible state(s) are affected.
    if state = -1, then all states are affected.

        '''
        r = DEFAULT_ERROR
        object_mode = int(object_mode)
        if cmd.is_string(vector):
            vector = safe_list_eval(vector)
        if not cmd.is_list(vector):
            print "Error: bad vector."
            raise pymol.CmdException
        else:
            vector = [float(vector[0]),float(vector[1]),float(vector[2])]
            selection = selector.process(selection)
            camera=int(camera)
            view = cmd.get_view(0)
            if camera:
                mat = [ view[0:3],view[3:6],view[6:9] ]
                shift = cpv.transform(mat,vector)
            else:
                shift = vector
            if object==None:
                ttt = [1.0,0.0,0.0,shift[0],
                         0.0,1.0,0.0,shift[1],
                         0.0,0.0,1.0,shift[2],
                         0.0,0.0,0.0,1.0]
                r=cmd.transform_selection(selection,ttt,state=state)
            elif object_mode==0: # update the TTT display matrix
                try:
                    lock()
                    r=_cmd.translate_object_ttt(str(object),shift)
                finally:
                    unlock(r)
            elif object_mode==1: # either updates TTT or coordinates & history
                     # depending on the current matrix mode
                matrix = [1.0, 0.0, 0.0, shift[0],
                          0.0, 1.0, 0.0, shift[1],
                          0.0, 0.0, 1.0, shift[2],
                          0.0, 0.0, 0.0, 1.0]
                try:
                    lock()
                    r = _cmd.transform_object(str(object),int(state)-1,
                                                      list(matrix),0,'',1)
                finally:
                    unlock(r)
            else:
                print " Error: translate: unrecognized object_mode"
        if _raising(r): raise pymol.CmdException            
        return r

    def rotate(axis='x',angle=0.0,selection="all",
                  state=0,camera=1,object=None,origin=None,object_mode=0):
        '''
DESCRIPTION

    "rotate" can be used to rotate the atomic coordinates of a
    molecular object.  Behavior differs depending on whether or not the
    "object" parameter is specified.

    If object is None, then rotate rotates the atomic coordinates
    according to the axes and angle for the selection and state
    provided.  All representation geometries will need to be
    regenerated to reflect the new atomic coordinates.

    If object is set to an object name, then selection and state are
    ignored and instead of translating the atomic coordinates, the
    object\'s representation display matrix is modified.  This option
    is for use in animations only.

USAGE

    rotate axis, angle [,selection [,state [,camera [,object [,origin]]]]]
    
PYMOL API

    cmd.rotate(list-or-string axis, float angle,
               string selection = "all", int state = 0,
                  int camera = 1, string object = None)

EXAMPLES

    rotate x, 45, pept

NOTES

    if state = 0, then only visible state(s) are affected.
    if state = -1, then all states are affected.

        '''
        r = DEFAULT_ERROR
        object_mode = int(object_mode)
        have_origin = 0
        if axis in ['x','X']:
            axis = [1.0,0.0,0.0]
        elif axis in ['y','Y']:
            axis = [0.0,1.0,0.0]
        elif axis in ['z','Z']:
            axis = [0.0,0.0,1.0]
        else:
            axis = safe_list_eval(str(axis))
        if not cmd.is_list(axis):
            print "Error: bad axis."
            raise pymol.CmdException
        else:
            axis = [float(axis[0]),float(axis[1]),float(axis[2])]
            angle = math.pi*float(angle)/180.0
            view = cmd.get_view(0)
            if origin!=None:
                have_origin = 1
                if cmd.is_string(origin):
                    if ',' in origin:
                        origin = safe_list_eval(origin) # should be a sequence of floats
                    else:
                        lock()
                        try:
                            origin = _cmd.get_origin(str(origin))
                        finally:
                            unlock(-1)
                origin = [float(origin[0]),float(origin[1]),float(origin[2])]
            else:
                origin = [view[12],view[13],view[14]]
            camera=int(camera)
            if camera:
                vmat = [ view[0:3],view[3:6],view[6:9] ]
                axis = cpv.transform(vmat,axis)
            mat = cpv.rotation_matrix(angle,axis)
            if object==None:
                ttt = [mat[0][0],mat[0][1],mat[0][2],origin[0],                   
                       mat[1][0],mat[1][1],mat[1][2],origin[1],
                       mat[2][0],mat[2][1],mat[2][2],origin[2],
                       -origin[0],-origin[1],-origin[2], 1.0]
                r=cmd.transform_selection(selection,ttt,state=state)
            elif object_mode==0:
                lock()
                try:
                    if not have_origin:
                        origin = _cmd.get_origin(str(object))
                    if is_sequence(origin):
                        ttt = [mat[0][0],mat[0][1],mat[0][2], origin[0],
                               mat[1][0],mat[1][1],mat[1][2], origin[1],
                               mat[2][0],mat[2][1],mat[2][2], origin[2],
                               -origin[0], -origin[1], -origin[2], 1.0]
                        r=_cmd.combine_object_ttt(str(object),ttt)
                finally:
                    unlock(r)
                if not is_sequence(origin):
                    print " Error: rotate: unknown object '%s'."%object
                    if _raising(r): raise pymol.CmdException                                
            elif object_mode==1:
                
                matrix = [mat[0][0],mat[0][1],mat[0][2], origin[0],     
                          mat[1][0],mat[1][1],mat[1][2], origin[1],
                          mat[2][0],mat[2][1],mat[2][2], origin[2],
                          -origin[0],-origin[1],-origin[2], 1.0]
                try:
                    lock()
                    r = _cmd.transform_object(str(object),int(state)-1,
                                                      list(matrix),0,'',0)
                finally:
                    unlock(r)
                
            else:
                print " Error: rotate: unrecognized object_mode"
        if _raising(r): raise pymol.CmdException            
        return r


    def set_title(object,state,text):
        '''
DESCRIPTION

    "set_title" attaches a text string to the state of a particular
    object which can be displayed when the state is active.  This is
    useful for display the energies of a set of conformers.

USAGE

    set_title object,state,text

PYMOL API

    cmd.set_title(string object,int state,string text)

    '''
        r = DEFAULT_ERROR
        try:
            lock()
            r = _cmd.set_title(str(object),int(state)-1,str(text))
        finally:
            unlock(r)

    def set_object_ttt(object,ttt,state=0,quiet=1,homogenous=0):
        r = None
        if cmd.is_string(ttt):
            ttt = safe_list_eval(str(ttt))
        if homogenous: # passed a homogenous matrix, so do the best we can
            ttt = [
                ttt[ 0], ttt[ 4], ttt[ 8], 0.0,
                ttt[ 1], ttt[ 5], ttt[ 9], 0.0,
                ttt[ 2], ttt[ 6], ttt[10], 0.0,
                ttt[ 3], ttt[ 7], ttt[11], 1.0]
        try:
            lock()
            r = _cmd.set_object_ttt(str(object),
                                            (
                float(ttt[ 0]),
                float(ttt[ 1]),
                float(ttt[ 2]),
                float(ttt[ 3]),            
                float(ttt[ 4]),
                float(ttt[ 5]),
                float(ttt[ 6]),
                float(ttt[ 7]),            
                float(ttt[ 8]),
                float(ttt[ 9]),
                float(ttt[10]),
                float(ttt[11]),            
                float(ttt[12]),
                float(ttt[13]),
                float(ttt[14]),
                float(ttt[15])),
                                            int(state)-1,int(quiet))
        finally:
            unlock(r)
        if _raising(r): raise pymol.CmdException            
        return r

    def transform_selection(selection,matrix,state=0,log=0,homogenous=0,transpose=0):
        '''

"transform_selection" is UNSUPPORTED.

cmd.transform_selection(string selection, list-of-16-floats matrix, int state-number):

Note that matrix is NOT a standard homogenous 4x4 transformation
matrix.  Instead it is something PyMOL-specific which consists of the
following:

1) a 3x3 matrix containing the rotation in the upper-left quadrant

2) a 1x3 translation to be applied *before* rotation in the bottom row
    (matrix[12],matrix[13],matrix[14]).

3) a 3x1 translation to be applied *after* rotation in the right-hand
    column (matrix[3],matrix[7],matrix[11])

In other words, if the matrix is:

[  m0  m1  m2  m3
    m4  m5  m6  m7
    m8  m9 m10 m11
  m12 m13 m14 m15 ]

Atoms will be transformed as follows

Y = M X

y0 = m0*(x0+m12) + m1*(x1+m13) +  m8*(x2+m14) + m3
y1 = m4*(x0+m12) + m5*(x1+m13) +  m9*(x2+m14) + m7
y2 = m8*(x0+m12) + m9*(x1+m13) + m10*(x2+m14) + m11

        '''
        r = DEFAULT_ERROR
        selection = selector.process(selection)                
        if int(transpose):
            matrix = [ matrix[0], matrix[4], matrix[8 ], matrix[12],
                          matrix[1], matrix[5], matrix[9 ], matrix[13],
                          matrix[2], matrix[6], matrix[10], matrix[14],
                          matrix[3], matrix[7], matrix[11], matrix[15]]
        try:
            lock()
            r = _cmd.transform_selection(str(selection),int(state)-1,
                                                  list(matrix),int(log),int(homogenous))
        finally:
            unlock(r)
        if _raising(r): raise pymol.CmdException            
        return r

    def transform_object(name,matrix,state=0,log=0,selection='',homogenous=0,transpose=0):
        r = DEFAULT_ERROR
        if int(transpose):
            matrix = [ matrix[0], matrix[4], matrix[8 ], matrix[12],
                          matrix[1], matrix[5], matrix[9 ], matrix[13],
                          matrix[2], matrix[6], matrix[10], matrix[14],
                          matrix[3], matrix[7], matrix[11], matrix[15]]
        try:
            lock()
            r = _cmd.transform_object(str(name),int(state)-1,
                                              list(matrix),int(log),
                                              str(selection),
                                              int(homogenous))
        finally:
            unlock(r)
        if _raising(r): raise pymol.CmdException            
        return r

    def matrix_copy(source_name,    target_name,
                    source_mode=-1,  target_mode=-1,
                    source_state=1, target_state=1,
                    target_undo=1, log=0, quiet=1):
        '''

DESCRIPTION
        
    "matrix_copy" copies a transformation matrix from one object to
    another. This command is often used after a protein structure
    alignment to bring other related objects into the same frame of
    reference.  Common Usage

USAGE

    matrix_copy source_name, target_name

SEE ALSO

    matrix_reset, align, fit, pair_fit
'''
        
        r = DEFAULT_ERROR
        try:
            lock()
            r = _cmd.matrix_copy(str(source_name),
                                             str(target_name),
                                             int(source_mode),
                                             int(target_mode),
                                             int(source_state)-1,
                                             int(target_state)-1,
                                             int(target_undo),
                                             int(log),
                                             int(quiet))
        finally:
            unlock(r)
        if _raising(r): raise pymol.CmdException            
        return r

    def matrix_reset(name, state=1, mode=-1, log=0, quiet=1):
        r = DEFAULT_ERROR
        try:
            lock()
            r = _cmd.reset_matrix(str(name),
                                         int(mode),
                                         int(state)-1,
                                         int(log),
                                         int(quiet))
        finally:
            unlock(r)
        if _raising(r): raise pymol.CmdException            
        return r

        
    def translate_atom(sele1,v0,v1,v2,state=0,mode=0,log=0):
        r = DEFAULT_ERROR
        sele1 = selector.process(sele1)
        try:
            lock()
            r = _cmd.translate_atom(str(sele1),float(v0),float(v1),
                                            float(v2),int(state)-1,int(mode),int(log))
        finally:
            unlock(r)
        if _raising(r): raise pymol.CmdException            
        return r

    def update(target,source,target_state=0,source_state=0,matchmaker=1,quiet=1):
        '''
DESCRIPTION

    "update" transfers coordinates from one selection to another.
USAGE

    update (target-selection),(source-selection)

EXAMPLES

    update target,(variant)

NOTES

    Currently, this applies across all pairs of states.  Fine
    control will be added later.

SEE ALSO

    load
    '''
        r = DEFAULT_ERROR
        a=target
        b=source
        # preprocess selections
        a = selector.process(a)
        b = selector.process(b)
        #
        if a[0]!='(': a="("+str(a)+")"
        if b[0]!='(': b="("+str(b)+")"   
        try:
            lock()   
            r = _cmd.update(str(a),str(b),int(target_state)-1,
                                 int(source_state)-1,int(matchmaker),int(quiet))
        finally:
            unlock(r)
        if _raising(r): raise pymol.CmdException            
        return r


    def set_dihedral(atom1,atom2,atom3,atom4,angle,state=1,quiet=1):
        # preprocess selections
        atom1 = selector.process(atom1)
        atom2 = selector.process(atom2)
        atom3 = selector.process(atom3)
        atom4 = selector.process(atom4)
        #   
        r = DEFAULT_ERROR
        try:
            lock()
            r = _cmd.set_dihe(str(atom1),str(atom2),str(atom3),str(atom4),
                                    float(angle),int(state)-1,int(quiet))
        finally:
            unlock(r)
        if _raising(r): raise pymol.CmdException            
        return r

    map_op_dict = {
        'minimum'       : 0,
        'maximum'       : 1,
        'sum'           : 2,
        'average'       : 3,
        'difference'    : 4,
        'copy'          : 5,
        'unique'        : 6,
        }
    
    map_op_sc = Shortcut(map_op_dict.keys())

    def map_set(name, operator, operands='', target_state=0, source_state=0, zoom=0, quiet=1):
        '''
DESCRIPTION

    "map_set" provides a number of common operations on and between maps.

USAGE

    map_set name, operator, operands, target_state, source_state

    operator may be "minimum, maximum, average, sum, or difference"

EXAMPLES

    map my_sum, add, map1 map2 map3
    map my_avg, average, map1 map2 map3
    
NOTES

    source_state = 0 means all states
    target_state = -1 means current state
    
    experimental
    
SEE ALSO

    map_new
        '''
        r = DEFAULT_ERROR
        operator_index = map_op_dict[map_op_sc.auto_err(operator,'operator')]
        try:
            lock()
            r = _cmd.map_set(str(name), int(operator_index), str(operands),
                         int(target_state)-1, int(source_state)-1, int(zoom), int(quiet))
        finally:
            unlock(r)
        if _raising(r): raise pymol.CmdException            
        return r
            
    def map_set_border(name,level=0.0,state=0):
        '''
DESCRIPTION

    "map_set_border" is a function (reqd by PDA) which allows you to set the
    level on the edge points of a map

USAGE

    map_set_border name, level

NOTES

    unsupported.
    
SEE ALSO

    load
        '''
        r = DEFAULT_ERROR
        try:
            lock()
            r = _cmd.map_set_border(str(name),float(level),int(state))
        finally:
            unlock(r)
        if _raising(r): raise pymol.CmdException            
        return r

    def map_double(name,state=0):
        '''
DESCRIPTION

    "map_double" resamples a map at twice the current resolution.  The
    amount of memory required to store the map will increase
    eight-fold.

USAGE

    map_double map_name, state
        '''
        r = DEFAULT_ERROR
        try:
            lock()
            r = _cmd.map_double(str(name),int(state)-1)
        finally:
            unlock(r)
        if _raising(r): raise pymol.CmdException            
        return r

    def map_halve(name,state=0,smooth=1):
        '''
DESCRIPTION

    "map_halve" resamples a map at half the current resolution.  The
    amount of memory required to store the map will decrease
    eight-fold.

USAGE

    map_halve map_name, state
        '''
        r = DEFAULT_ERROR
        try:
            lock()
            r = _cmd.map_halve(str(name),int(state)-1,int(smooth))
        finally:
            unlock(r)
        if _raising(r): raise pymol.CmdException            
        return r

    def map_trim(name,selection,buffer=0.0,map_state=0,sele_state=0,quiet=1):
        r = DEFAULT_ERROR
        # preprocess selection
        selection = selector.process(selection)
        #   
        try:
            lock()
            r = _cmd.map_trim(str(name),str(selection),
                                  float(buffer),int(map_state)-1,
                                  int(sele_state)-1,int(quiet))
        finally:
            unlock(r)
        if _raising(r): raise pymol.CmdException            
        return r

    
    def protect(selection="(all)",quiet=1):
        '''
DESCRIPTION

    "protect" protects a set of atoms from tranformations performed
    using the editing features.  This is most useful when you are
    modifying an internal portion of a chain or cycle and do not wish
    to affect the rest of the molecule.

USAGE

    protect (selection)

PYMOL API

    cmd.protect(string selection)

SEE ALSO

    deprotect, mask, unmask, mouse, editing
    '''
        r = DEFAULT_ERROR
        # preprocess selection
        selection = selector.process(selection)
        #   
        try:
            lock()   
            r = _cmd.protect("("+str(selection)+")",1,int(quiet))
        finally:
            unlock(r)
        if _raising(r): raise pymol.CmdException            
        return r

    def deprotect(selection="(all)",quiet=1):
        '''
DESCRIPTION

    "deprotect" reveres the effect of the "protect" command.

USAGE

    deprotect (selection)

PYMOL API

    cmd.deprotect(string selection="(all)")

SEE ALSO

    protect, mask, unmask, mouse, editing
    '''
        r = DEFAULT_ERROR
        # preprocess selection
        selection = selector.process(selection)
        #   
        try:
            lock()   
            r = _cmd.protect("("+str(selection)+")",0,int(quiet))
        finally:
            unlock(r)
        if _raising(r): raise pymol.CmdException            
        return r

    flag_dict = {
    # simulation 
        'focus'         : 0,
        'free'          : 1,
        'restrain'      : 2,
        'fix'           : 3,
        'exclude'       : 4,
    # rendering
        'exfoliate'     : 24,
        'ignore'        : 25,
        'no_smooth'     : 26,
    }

    flag_sc = Shortcut(flag_dict.keys())

    flag_action_dict = {
        'reset' : 0,
        'set'   : 1,
        'clear' : 2,
        }

    flag_action_sc = Shortcut(flag_action_dict.keys())

    def fix_chemistry(selection1 = "all",selection2 = "all", invalidate=0, quiet=1):
        r = DEFAULT_ERROR
        selection1 = selector.process(selection1)
        selection2 = selector.process(selection2)
        try:
            lock()   
            r = _cmd.fix_chemistry("("+str(selection1)+")","("+str(selection2)+")",int(invalidate),int(quiet))
        finally:
            unlock(r)
        if _raising(r): raise pymol.CmdException            
        return r
    
    def flag(flag,selection,action="reset",quiet=1):
        '''
DESCRIPTION

    "flag" sets the indicated flag for atoms in the selection and
     clears the indicated flag for atoms not in the selection.  This
     is primarily useful for passing selection information into
     Chempy models, which have a 32 bit attribute "flag" which holds
     this information.

USAGE

    flag flag, selection [ ,action ]

    flag flag = selection [ ,action ]      # (DEPRECATED)

    action can be:
      "reset" (default) sets flag on selection, clear it on other atoms 
      "set" sets the flag for selected atoms, leaves other atoms unchanged
      "clear" clear the flag for selected atoms, leaves other atoms unchanged

PYMOL API

    cmd.flag( int flag, string selection, string action="reset",
                 int indicate=0)
    cmd.flag( string flag, string selection, string action="reset",
                 int indicate=0)

EXAMPLES  

    flag free, (resi 45 x; 6)

NOTE

    If the 'auto_indicate_flags' setting is true, then PyMOL will automatically
    create a selection called "indicate" which contains all atoms with that flag
    after applying the command.

RESERVED FLAGS

    Flags 0-7 are reserved for molecular modeling */
        focus      0 = Atoms of Interest (i.e. a ligand in an active site)
        free       1 = Free Atoms (free to move subject to a force-field)
        restrain   2 = Restrained Atoms (typically harmonically contrained)
        fix        3 = Fixed Atoms (no movement allowed)
        exclude    4 = Atoms which should not be part of any simulation
    Flags 8-15 are free for end users to manipulate
    Flags 16-23 are reserved for external GUIs and linked applications
    Flags 24-30 are reserved for PyMOL internal usage
        exfoliate 24 = Remove surface from atoms when surfacing
        ignore    25 = Ignore atoms altogether when surfacing
        no_smooth 26 = Don\'t smooth atom position
    Flag 31 is reserved for coverage tracking when assigning parameters, etc.
        '''
        r = DEFAULT_ERROR
        # preprocess selection
        new_flag = flag_sc.interpret(str(flag))
        if new_flag:
            if cmd.is_string(new_flag):
                flag = flag_dict[new_flag]
            else:
                flag_sc.auto_err(flag,'flag')
        # preprocess selection
        selection = selector.process(selection)
        action = flag_action_dict[flag_action_sc.auto_err(action,'action')]
        #      
        try:
            lock()
            r = _cmd.flag(int(flag),"("+str(selection)+")",
                              int(action),int(quiet))
        finally:
            unlock(r)
        if _raising(r): raise pymol.CmdException            
        return r

    def vdw_fit(selection1,selection2,state1=1,state2=1,buffer=0.24,quiet=1):
    #
    #   UNSUPPORTED FEATURE - LIKELY TO CHANGE
    #
        # preprocess selections
        selection1 = selector.process(selection1)
        selection2 = selector.process(selection2)
        #
        r = DEFAULT_ERROR
        try:
            lock()
            r = _cmd.vdw_fit(str(selection1),int(state1)-1,
                             str(selection2),int(state2)-1,
                             float(buffer),int(quiet))
        finally:
            unlock(r)
        if _raising(r): raise pymol.CmdException
        return r

    import pymol

