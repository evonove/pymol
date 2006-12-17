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

if __name__=='pymol.helping':
    
    import string
    import thread
    import cmd

    from cmd import DEFAULT_ERROR, DEFAULT_SUCCESS, _raising, is_ok, is_error
    def show_help(cmmd): # INTERNAL
        print "PyMOL>help %s" % cmmd
        help(cmmd)
        if cmd.get_setting_legacy("internal_feedback")>0.1:
            print "(Hit ESC to hide)"

    def help(command = "commands"):
        '''
DESCRIPTION

    "help" prints out the online help for a given command.

USAGE

    help command
        '''
        r = DEFAULT_SUCCESS
#        if cmd.get_setting_legacy("internal_feedback")>0.1:
#            cmd.set("text","1",quiet=1)
        cmmd = cmd.help_sc.auto_err(command,'topic')   
        if cmd.keyword.has_key(cmmd):
            doc = cmd.keyword[cmmd][0].__doc__
            if doc:
                print "\n",string.strip(doc),"\n"
            else:
                print "Error: sorry no help available on that command."
        elif cmd.help_only.has_key(cmmd):
            doc = cmd.help_only[cmmd][0].__doc__
            if doc:
                print "\n",string.strip(doc),"\n"
            else:
                print "Error: sorry no help available on that command."      
        else:
            print "Error: unrecognized command"
        return r

    def commands():
        '''
COMMANDS

    INPUT/OUTPUT  load      save      delete    quit
    VIEW          turn      move      clip      rock
                  show      hide      enable    disable
                  reset     refresh   rebuild   
                  zoom      origin    orient   
                  view      get_view  set_view
    MOVIES        mplay     mstop     mset      mdo
                  mpng      mmatrix   frame
                  rewind    middle    ending
                  forward   backward
    IMAGING       png       mpng
    RAY TRACING   ray       
    MAPS          isomesh   isodot
    DISPLAY       cls       viewport  splash    
    SELECTIONS    select    mask   
    SETTINGS      set       button
    ATOMS         alter     alter_state 
    EDITING       create    replace   remove    h_fill   remove_picked
                  edit      bond      unbond    h_add    fuse       
                  undo      redo      protect   cycle_valence  attach
    FITTING       fit       rms       rms_cur   pair_fit  
                  intra_fit intra_rms intra_rms_cur   
    COLORS        color     set_color
    HELP          help      commands
    DISTANCES     dist      
    STEREO        stereo
    SYMMETRY      symexp
    SCRIPTS       @         run
    LANGUAGE      alias     extend

Try "help <command-name>".  Also see the following extra topics:

    "movies", "keyboard", "mouse", "selections",
    "examples", "launching", "editing", and "api".
    '''
        cmd.help('commands')

    def editing():
        '''
SUMMARY

PyMOL has a rudimentary, but quite functional molecular structure
editing capability.  However, you will need to use an external mimizer
to "clean-up" your structures after editing.  Furthermore, if you are
going to modify molecules other than proteins, then you will also need
a way of assigning atom types on the fly.

To edit a conformation or structure, you first need to enter editing
mode (see Mouse Menu).  Then you can pick an atom (CTRL-Middle click)
or a bond (CTRL-Right click).  Next, you can use the other
CTRL-key/click combinations listed on the right hand side of the
screen to adjust the attached fragments.  For example, CTRL-left click
will move fragments about the selected torsion.

Editing structures is done through a series of CTRL key actions
applied to the currently selected atom or bonds. See "help edit_keys"
for the exact combinations.  To build structures, you usually just
replace hydrogens with methyl groups, etc., and then repeat.  They are
no short-cuts currently available for building common groups, but that
is planned for later versions.

NOTE

Only "lines" and "sticks" representations can be picked using the
mouse, however other representations will not interfere with picking
so long as one of these representation is present underneath.

    '''
        cmd.help('editing')

    def release():
        '''
RELEASE NOTES

PyMOL is a free, open, and expandable molecular graphics system
written by a computational scientist to enable molecular modeling from
directly within Python.  It will be of most benefit to hybrid
scientist/developers in the fields of structural biology,
computational chemistry, and informatics who seek an open and
unrestricted visualization tool for interfacing with their own
programs.  PyMOL will also be of benefit to advanced non-developers
familiar with similar programs such as Midas, O, Grasp, X-PLOR and
CNS.

Due to PyMOL's current "user-unfriendliness", this release is most
appropriate for those who prefer to use text commands and scripts, and
for developers who want to integrate PyMOL's visualization and
molecular editing capabilities with their own work.

PyMOL currently includes a diverse command language, a powerful
application programmers interface (API), and a variety of mouse and
keyboard driven functionality for viewing, animation, rendering, and
molecular editing.  A partial manual is now available on the web.

Two external GUI development options are supported for PyMOL:
"Tkinter" and "wxPython".  Developers can take their pick.  I am
committed to insuring that PyMOL will work with both of them, but it
is unlikely that I will have time to develop a complete external GUI
myself any time soon using either toolkit.

Note that only Tkinter is supported under Windows with the default
PyMOL and Python distributions, so for maximum ease of installation
under Windows, stick with Tkinter (Tcl/Tk).  For this reason, the
Tkinter-based GUI is going to be the default GUI for standard PyMOL
despite its drawbacks.

Warren L. DeLano (5/1/2001), warren@delanoscientific.com
    '''
        cmd.help('release')

    def edit_keys():
        '''
EDITING KEYS 

    These are defaults, which can be redefined.  Note that while
entering text on the command line, some of these control keys take on
text editing functions instead (CTRL - A, E, and K, and DELETE), so
you should clear the command line before trying to edit atoms.

ATOM REPLACEMENT

    CTRL-C    Replace picked atom with carbon   (C)
    CTRL-N    Replace picked atom with nitrogen (N)
    CTRL-O    Replace picked atom with oxygen   (O)
    CTRL-S    Replace picked atom with sulpher  (S)
    CTRL-G    Replace picked atom with hydrogen (H)
    CTRL-F    Replace picked atom with fluorene (F)
    CTRL-L    Replace picked atom with chlorine (Cl)
    CTRL-B    Replace picked atom with bromine  (Br)
    CTRL-I    Replace picked atom with iodine   (I)

ATOM MODIFICATION

    CTRL-J    Set charge on picked atom to -1
    CTRL-K    Set charge on picked atom to +1
    CTRL-D    Remove atom or bond (DELETE works too).
    CTRL-Y    Add a hydrogen to the current atom
    CTRL-R    Adjust hydrogens on atom/bond to match valence.
    CTRL-E    Inverts the picked stereo center, but you must first
                 indicate the constant portions with the (lb) and (rb)
                 selections.

    CTRL-T    Connect atoms in the (lb) and (rb) selections.
    CTRL-W    Cycle the bond valence on the picked bond.

UNDO and REDO of conformational changes (not atom changes!)

    CTRL-Z    undo the previous conformational change.
                 (you can not currently undo atom modifications).
    CTRL-A    redo the previous conformational change.

    '''
        cmd.help('edit_keys')


    def at_sign():
        '''
DESCRIPTION

    "@" sources a PyMOL command script as if all of the commands in the
    file were typed into the PyMOL command line.

USAGE

    @ <script-file>

PYMOL API

    Not directly available. Instead, use cmd.do("@...").

    '''
        cmd.help(at_sign)


    def run():
        '''
DESCRIPTION

    "run" executes an external Python script in a local name space, the
    global namespace, or in its own namespace (as a module).

USAGE

    run python-script [, (local | global | module | main | private ) ]

PYMOL API

    Not directly available.  Instead, use cmd.do("run ...").

NOTES

    The default mode for run is "global".

    Due to an idiosyncracy in Pickle, you can not pickle objects
    directly created at the main level in a script run as "module",
    (because the pickled object becomes dependent on that module).
    Workaround: delegate construction to an imported module.

    '''
        cmd.help(run)

    def spawn():
        '''
DESCRIPTION

    "spawn" launches a Python script in a new thread which will run
    concurrently with the PyMOL interpreter. It can be run in its own
    namespace (like a Python module, default), a local name space, or
    in the global namespace.

USAGE

    run python-script [, ( local | global | module | main | private )]

PYMOL API

    Not directly available.  Instead, use cmd.do("spawn ...").

NOTES

    The default mode for spawn is "module".

    Due to an idiosyncracy in Pickle, you can not pickle objects
    directly created at the main level in a script run as "module",
    (because the pickled object becomes dependent on that module).
    Workaround: delegate construction to an imported module.

    The best way to spawn processes at startup is to use the -l option
    (see "help launching").
    '''
        cmd.help(spawn)

    def api():
        '''
DESCRIPTION

    The PyMOL Python Application Programming Interface (API) should be
    accessed exclusively through the "cmd" module (never "_cmd"!).  Nearly
    all command-line functions have a corresponding API method.

USAGE

    from pymol import cmd
    result = cmd.<command-name>( argument , ... ) 

NOTES

    Although the PyMOL core is not multi-threaded, the API is
    thread-safe and can be called asynchronously by external python
    programs.  PyMOL handles the necessary locking to insure that
    internal states do not get corrupted.  This makes it very easy to
    build complicated systems which involve direct realtime visualization.

        '''

        cmd.help('api')

    def keyboard():
        '''
KEYBOARD COMMANDS and MODIFIERS

    ESC          Toggle onscreen text.
    INSERT       Toggle rocking.

    LEFT ARROW, RIGHT ARROW    Go backward or forward one frame, or when
                                        editing, go forward or back one character.
    HOME, END    Go to the beginning or end of a movie.

    Command Entry Field in the Interal GUI (black window)

    TAB          Complete commmand or filename (like in tcsh or bash).
    CTRL-A       Go to the beginning of the line.
    CTRL-E       Go to the end of the line.
    CTRL-K       Delete through to the end of the line.

    Command Entry Field on the External GUI (gray window).

    CTRL-C       These operating system-provided cut and paste functions
    CTRL-V       will only work in the external GUI command line.

EDITING 

    type "help edit_keys" for keyboard shortcuts used in editing.

        '''
        cmd.help('keyboard')

    def transparency():
        '''
TRANSPARENCY

    As of version 0.68, trasparent surfaces are supported in both
    realtime (OpenGL) rendering mode as well as with ray-traced images.

    Transparency is currently managed by setting either the global
    transparency variable or one attached to an individual molecule object.

    It isn't yet possible to control transparency on a per-atom basis.

EXAMPLES

    set transparency=0.5        # makes all surfaces 50% transparent
    set transparency=0.5, mol3  # makes only mol3's surface transparent

        '''
        cmd.help('transparency')


    def mouse():
        '''
MOUSE CONTROLS

  The configuration can be changed using the "Mouse" menu.  The
  current configuration is described on screen with a small matrix on
  the lower right hand corner, using the following abbreviations:

    Buttons (Horizontal Axis)

        L        = left mouse click
        M        = middle mouse click
        R        = right mouse click

    Modifiers (Veritical axis on the matrix) 

        None     = no keys held down while clicking
        Shft     = hold SHIFT down while clicking
        Ctrl     = hold CTRL down while clicking
        CtSh     = hold both SHIFT and CTRL down while clicking

    Visualization Functions

        Rota     = Rotates camera about X, Y, and Z axes
        RotZ     = Rotates camera about the Z axis
        Move     = Translates along the X and Y axes
        MovZ     = Translates along Z axis
        Clip     = Y motion moves the near clipping plane while
        PkAt     = Pick an atom
        PkBd     = Pick a bond
        Orig     = Move origin to selected atom
        +lb      = Add an atom into the (lb) selection
        lb       = Define the (lb) selection with the indicated atom.
        rb       = Define the (rb) selection with the indicated atom.

    Editing Functions

        RotF     = Rotate fragment
        MovF     = Move fragment
        TorF     = Torsion fragment
    '''
        cmd.help('mouse')

    def examples():
        '''
EXAMPLE ATOM SELECTIONS

    select bk = ( name ca or name c or name n )
        * can be abbreviated as *
    sel bk = (n;ca,c,n)

    select hev = ( not hydro )
        * can be abbreviated as *
    sel hev = (!h;)

    select site = ( byres ( resi 45:52 expand 5 ))
        * can be abbreviated as *
    sel site = (b;(i;45:52 x;5))

    select combi = ( hev and not site )
        * can be abbreviated as *
    sel combi = (hev&!site)
        '''
        cmd.help('examples')

    def launching():
        '''
PyMOL COMMAND LINE OPTIONS 

    -c   Command line mode, no GUI.  For batch opeations.
    -i   Disable the internal OpenGL GUI (object list, menus, etc.)
    -x   Disable the external GUI module.
    -t   Use Tcl/Tk based external GUI module (pmg_tk).
    -q   Quiet launch. Suppress splash screen & other chatter.
    -p   Listen for commands on standard input.
    -e   Start in full-screen mode.
    -2   Start in two-button mouse mode.
    -o   Disable security protections for session files.
    -R   Launch Greg Landrum's XMLRPC listener.
    -B   Enable blue-line stereo signal (for Mac stereo)
    -G   Start in Game mode.
    -S   Force and launch in stereo, if possible.
    -M   Force mono even when hardware stereo is present.

    -X <int> -Y <int> -W <int> -H <int> -V <int> Adjust window geometry.

    -f <# line> Controls display of commands and feedback in OpenGL (0=off).
    -r <file.py> Run a Python program (in __main__) on startup.
    -l <file.py> Spawn a python program in new thread.
    -d <string> Run pymol command string upon startup.
    -u <script> Load and append to this PyMOL script or program file.
    -s <script> Save commands to this PyMOL script or program file.
    -g <file.png> Write a PNG file (after evaluating previous arguments)
    
    <file> can have one of the following extensions, and all 
    files provided will be loaded or run after PyMOL starts.

     .pml            PyMOL command script to be run on startup
     .py, .pym, .pyc Python program to be run on startup
     .pdb            Protein Data Bank format file to be loaded on startup
     .mmod           Macromodel format to be loaded on startup
     .mol            MDL MOL file to be loaded on startup
     .sdf            MDL SD file to be parsed and loaded on startup
     .xplor          X-PLOR Map file (ASCII) to be loaded on startup
     .ccp4           CCP4 map file (BINARY) to be loaded on startup
     .cc1, .cc2      ChemDraw 3D cartesian coordinate file
     .pkl            Pickled ChemPy Model (class "chempy.model.Indexed")
     .r3d            Raster3D file
     .cex            CEX file (Metaphorics)
     .top            AMBER topology file
     .crd            AMBER coordinate file
     .rst            AMBER restart file
     .trj            AMBER trajectory
     .pse            PyMOL session file
     .phi            Delphi/Grasp Electrostatic Potential Map
        '''
        cmd.help('launching')

    def movies():
        '''
MOVIES

    To create a movie, simply load multiple coordinate files
    into the same object.  This can be accomplish at the command line,
    using script files, or by writing PyMOL API-based programs.

    The commands:

load frame001.pdb,mov
load frame002.pdb,mov

    will create a two frame movie.  So will the following program:

from pymol import cmd

for a in ( "frame001.pdb","frame002.pdb" ):
    cmd.load(a,"mov")

    which can be executed at the command line using the "run" command.

    Python built-in glob module can be useful for loading movies.

from pymol import cmd
import glob
for a in ( glob.glob("frame*.pdb") ):
    cmd.load(a,"mov")

NOTE

    Because PyMOL stores all movie frames in memory, there is a
    a practical limit to the number of atoms in all coordinate files. 
    160 MB free RAM enables 500,000 atoms with line representations.
    Complex representations require significantly more memory.
        '''
        cmd.help('movies')

    ### -------------------------------------------------------------------
    def selections():
        '''
DESCRIPTION

    Selections are enclosed in parentheses and contain predicates,
    logical operations, object names, selection names and nested
    parenthesis: ( [... [(...) ... ]] )

        name <atom names>            n. <atom names>          
        resn <residue names>         r. <residue names>
        resi <residue identifiers>   i. <residue identifiers>
        chain <chain ID>             c. <chain identifiers>
        segi <segment identifiers>   s. <segment identifiers>
        elem <element symbol>        e. <element symbols>
        flag <number>                f. <number>
        alt <code>                   
        numeric_type <numeric type>  nt. <numeric type>
        text_type <text type>        tt. <text type>
        b <operator> <value>         
        q <operator> <value>         
        formal_charge <op> <value>   fc. <operator> <value>
        partial_charge <op> <value>  pc. <operator> <value>
        id <original-index>          
        hydrogen                     h.
        all                          *
        visible                      v.
        hetatm                       
        <selection> and <selection>  <selection> & <selection>
        <selection> or <selection>   <selection> | <selection>
        not <selection>              ! <selection>
        byres <selection>            br. <selection>
        byobj <selection>            bo. <selection>
        around <distance>            a. <distance>
        expand <distance>            e. <distance>
        gap <distance>               
        in <selection>               
        like <selection>             l. <selection>
        <selection> within <distance> of <selection>
                              <selection> w. <distance> of <selection>      
        '''
        cmd.help('selections')

    def povray():
        '''
DESCRIPTION

    PovRay: Persistance of Vision Support Information (EXPERIMENTAL)

    The built-in ray-tracer (technically a ray-caster) is as fast or
    faster than PovRay for most figures (provided that hash_max is
    tuned for your system).  However, PovRay blows PyMOL away when it
    comes to rendering images without using lots of RAM, and with
    PovRay you get the ability use perspective, textures, reflections,
    infinite objects, and a superior lighting model.

    PovRay does not include interpolated colors within triangles (!!),
    so you must patch the source code in order to obtain this basic
    functionality required for rendering molecular surfaces.  Details
    can be found on the DINO website.

        http://www.biozentrum.unibas.ch/~xray/dino

    To use PovRay, you must be running under Unix and have x-povray in
    your path.  Unfortunately, the developers of PovRay do not share
    PyMOL's open-source philosophy, so you will need to download,
    configure, patch (for smooth_color_triangles), and install it
    yourself.  Despite being free and open-source, PovRay's license
    prevents it from being modified or conveniently combined with
    PyMOL, and thus it serves as a textbook example for why open-source
    licenses should be wholly non-restrictive.

    Assuming that PovRay is built and in your path...

    ray renderer=1   # will use PovRay instead of the built-in engine

    set ray_default_renderer=1 # changes the default renderer to PovRay
    ray                        # will now use PovRay by default

    cmd.get_povray() # will give you a tuple of PovRay input strings
                          # which you can manipulate from Python

    set smooth_color_triangles = 1 # is required in order to get decent
        surfaces in PovRay.  You must patch PovRay first.

        '''
        cmd.help('povray')


    def faster():
        '''
RAY TRACING OPTIMIZATION

    1. Reduce object complexity to a minimum acceptable level.
            For example, try lowering:
                "cartoon_sampling" 
                "ribbon_sampling", and
                "surface_quality", as appropriate.

    2. Increase "hash_max" so as to obtain a voxel dimensions of
        0.3-0.6.  Proper tuning of "hash_max" can speed up
        rendering by a factor of 2-5X for non-trivial scenes.

        WARNING: memory usage depends on hash_max^3, so avoid
        pushing into virtual memory.  Roughly speaking:

            hash_max = 80  -->   ~9 MB hash + data
            hash_max = 160 -->  ~72 MB hash + data
            hash_max = 240 --> ~243 MB hash + data

        Avoid utilizing virtual memory for the voxel hash,
        it will slow things way down.

    3. Recompiling with optimizations on usually gives a 25-33%
        performance boost for ray tracing.

        '''

        help('faster')

    def abort():
        '''
DESCRIPTION

    "abort" abruptly terminates execution of the PyMOL command script
    without executing any additional commands.

SEE ALSO

    embed, skip, python
    '''

    def skip():
        '''
DESCRIPTION

    "skip" delimits a block of commands that are skipped instead of
    being executed.  

EXAMPLE


    skip

    # the following command will not be executed
    color blue, all
    
    skip end

NOTES

    If the "skip" command is commented out, the subsequent "skip end"
    can be left in place, and will have no effect upon execution of
    subsequent command.s
    
SEE ALSO

    abort, embed, python
    '''
        return None

    def python():
        '''
DESCRIPTION

    "python" delimits a block of literal Python code embedded in a
    PyMOL command script.

EXAMPLE

    python

    for a in range(1,10):
        b = 10 - a
        print a, b

    python end

NOTES

    Literal Python blocks avoid the annoying requirement of having to
    use explicit line continuation markers for multi-line Python
    commands embedded within Python scripts.  
    
SEE ALSO

    abort, embed, skip
    '''
        return None
