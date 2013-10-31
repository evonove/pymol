
# constant objects 

import cmd
import parsing
import re
import string
from cmd import Shortcut

gz_ext_re = re.compile(r"(\.?gz|\.bz2)$", re.I)

file_ext_re = re.compile(string.join([
    "\.pdb$|\.pdb1$|\.ent$|\.mol$|\.p5m$|",
    r"\.mmod$|\.mmd$|\.dat$|\.out$|\.mol2$|",
    r"\.xplor$|\.pkl$|\.sdf$|\.pqr|", 
    r"\.r3d$|\.xyz$|\.xyz_[0-9]*$|", 
    r"\.cc1$|\.cc2$|", # ChemDraw 3D
    r"\.cif$|", # CIF/mmCIF
    r"\.cube$|", # Gaussian Cube
    r"\.dx$|", # DX files (APBS)
    r"\.pse$|\.psw$|\.pze$|\.pzw$|", # PyMOL session (pickled dictionary) and gzipped
    r"\.pmo$|", # Experimental molecular object format
    r"\.moe$|", # MOE (proprietary)
    r"\.mae$|", # MAE (proprietary)
    r"\.ccp4$|", # CCP4
    r"\.top$|", # AMBER Topology
    r"\.trj$|", # AMBER Trajectory
    r"\.crd$|", # AMBER coordinate file
    r"\.rst7?$|", # AMBER restart
    r"\.cex$|", # CEX format (used by metaphorics)
    r"\.phi$|", # PHI format (delphi)
    r"\.fld$|", # FLD format (AVS)
    r"\.trj$|\.trr$|\.xtc$|\.gro$|\.g96$|\.dcd$|", # Trajectories
    r"\.o$|\.omap$|\.dsn6$|\.brix$|", # BRIX/O format
    r"\.grd$|", # InsightII Grid format
    r"\.acnt$", # Tripos / Sybyl ACNT format
    ],''), re.I)

class loadable:
    pdb = 0
    mol = 1
    molstr = 3
    mmod = 4
    mmodstr = 6
    xplor = 7
    model = 8
    pdbstr = 9    
    brick = 10    # chempy.brick object
    map = 11      # chempy.map object
    callback = 12 # pymol callback obejct
    cgo = 13      # compiled graphic object
    r3d = 14      # r3d, only used within cmd.py
    xyz = 15      # xyz, tinker format
    sdf1 = 16     # sdf, only used within cmd.py
    cc1 = 17      # cc1 and cc2, only used within cmd.py
    ccp4 = 18     # CCP4 map, under development
    pmo = 19      # pmo, experimental molecular object format
    cex = 20      # cex format
    top = 21      # AMBER topology
    trj = 22      # AMBER trajectory
    crd = 23      # AMBER coordinate
    rst = 24      # AMBER restart
    pse = 25      # PyMOL session
    xplorstr = 26 # XPLOR map as string
    phi = 27      # Delphi/Grasp
    fld = 28      # AVS field format (not yet general -- only uniform allowed)
    brix = 29     # BRIX/DSN6/O map format
    grd = 30      # Insight II Grid format
    pqr = 31      # PQR file (modified PDB file for APBS)
    dx = 32       # DX file (APBS)
    mol2 = 33     # MOL2 file (TRIPOS)
    mol2str = 34  # MOL2 file string (TRIPOS)
    p1m = 35      # P1M file (combined data & secure commands)
    ccp4str = 36  # CCP4 map string
    sdf = 37      # new default...
    sdf2 = 37     # SDF using C-based SDF parser (instead of Python)
    sdf2str = 38  # SDF ditto
    png = 39      # png image
    psw = 40      #
    moe = 41      # Chemical Computing Group ".moe" format (proprietary)
    xtc = 42      # xtc trajectory format (via plugin)
    trr = 43      # trr trajectory format (via plugin)
    gro = 44      # gro trajectory format (via plugin)
    trj2 = 45     # trj trajectroy format (via plugin)
    g96 = 46      # g96 trajectory format (via plugin)
    dcd = 47      # dcd trajectory format (via plugin)
    cube = 48     # cube volume file (via plugin)
    mae = 49      # Schrodinger ".mae" format (proprietary)
    cif1 = 50     # Python-based CIF parser
    phistr = 51   # electrostatic map as a string
    pim = 52      # General-purpose programmatic import (powerful, insecure)
    pwg = 53      # PyMOL web gui launch script
    aln = 54      # CLUSTALW alignment file
    fasta = 55    # FASTA sequence file
    acnt = 56     # Tripos/Sybyl acnt grid file (proposed)
    dtr = 57      # DESRES / Desmond
    pze = 58
    pzw = 59

_load2str = { loadable.pdb : loadable.pdbstr,
              loadable.mol : loadable.molstr,
              loadable.xplor : loadable.xplorstr,
              loadable.mol2 : loadable.mol2str,
              loadable.mmod : loadable.mmodstr,
              loadable.ccp4 : loadable.ccp4str,
              loadable.sdf2 : loadable.sdf2str}

safe_oname_re = re.compile(r"\ |\(|\)|\||\&|\!|\,")  # quash reserved characters
sanitize_list_re = re.compile(r"[^0-9\.\-\[\]\,]+")
sanitize_alpha_list_re = re.compile(r"[^a-zA-Z0-9_\'\"\.\-\[\]\,]+")
nt_hidden_path_re = re.compile(r"\$[\/\\]")
quote_alpha_list_re = re.compile(
    r'''([\[\,]\s*)([a-zA-Z_][a-zA-Z0-9_\ ]*[a-zA-Z0-9_]*)(\s*[\,\]])''')

def safe_alpha_list_eval(st):
    st = sanitize_alpha_list_re.sub('',st)
    st = quote_alpha_list_re.sub(r'\1"\2"\3',st) # need to do this twice
    st = quote_alpha_list_re.sub(r'\1"\2"\3',st)
    return eval(sanitize_alpha_list_re.sub('',st)) 

class SafeEvalNS(object):
    def __getitem__(self, name):
        return name

def safe_eval(st):
    return eval(st, {}, SafeEvalNS())

safe_list_eval = safe_eval

QuietException = parsing.QuietException

DEFAULT_ERROR = -1
DEFAULT_SUCCESS = None

#--------------------------------------------------------------------
# shortcuts...

toggle_dict = {'on':1,'off':0,'1':1,'0':0,'toggle':-1, '-1':-1}
toggle_sc = Shortcut(toggle_dict.keys())

stereo_dict = {'on':-2,'off':0,'0':0,'1':-2,'swap':-1,
               'quadbuffer':1,'crosseye':2,
               'walleye':3,'geowall':4,'sidebyside':5,
               'byrow':6, 'bycolumn':7, 'checkerboard':8, 
               'custom': 9, 'anaglyph' : 10, 
               'dynamic' : 11, 'clonedynamic': 12 }

stereo_sc = Shortcut(stereo_dict.keys())

space_sc = Shortcut(['cmyk','rgb','pymol'])

window_dict = { 'show' : 1, 'hide' : 0, 'position' : 2, 'size' : 3,
                'box' : 4, 'maximize' : 5, 'fit' : 6, 'focus' : 7,
                'defocus' : 8 }
window_sc = Shortcut(window_dict.keys())

repres = {
    'everything'    : -1,
    'sticks'        : 0,
    'spheres'       : 1,
    'surface'       : 2,
    'labels'        : 3,
    'nb_spheres'    : 4,
    'cartoon'       : 5,
    'ribbon'        : 6,
    'lines'         : 7,
    'mesh'          : 8,
    'dots'          : 9,
    'dashes'        :10,
    'nonbonded'     :11,
    'cell'          :12,
    'cgo'           :13,
    'callback'      :14,
    'extent'        :15,
    'slice'         :16,
    'angles'        :17,
    'dihedrals'     :18,
    'ellipsoids'    :19,
    'volume'        :20,
}
repres_sc = Shortcut(repres.keys())

boolean_dict = {
    'yes'           : 1,
    'no'            : 0,
    '1'             : 1,
    '0'             : 0,
    'on'            : 1,
    'off'           : 0
    }

boolean_sc = Shortcut(boolean_dict.keys())

from constants_palette import palette_dict

palette_sc = Shortcut(palette_dict.keys())


location_code = {
    'first' : -1,
    'top' : -1,
    'upper' : -2,
    'current' : 0,
    'bottom' : 1,
    'last' : 1
    }
location_sc = Shortcut(location_code.keys())

class fb_action:
    set = 0
    enable = 1
    disable = 2
    push = 3
    pop = 4

class fb_module:

# This first set represents internal C systems

    all                       =0
    isomesh                   =1
    map                       =2
    matrix                    =3
    mypng                     =4
    triangle                  =5
    match                     =6
    raw                       =7
    isosurface                =8
    opengl                    =9

    color                     =10
    cgo                       =11
    feedback                  =12
    scene                     =13
    threads                   =14  
    symmetry                  =15
    ray                       =16
    setting                   =17
    object                    =18
    ortho                     =19
    movie                     =20
    python                    =21
    extrude                   =22
    rep                       =23
    shaker                    =24

    coordset                  =25
    distset                   =26
    gadgetset                 =27

    objectmolecule            =30
    objectmap                 =31
    objectmesh                =32
    objectdist                =33 
    objectcgo                 =34
    objectcallback            =35
    objectsurface             =36
    objectgadget              =37
    objectslice               =38
    objectvolume              =39

    repangle                  =43
    repdihederal              =44
    repwirebond               =45
    repcylbond                =46
    replabel                  =47
    repsphere                 =49
    repsurface                =50
    repmesh                   =51
    repdot                    =52
    repnonbonded              =53
    repnonbondedsphere        =54
    repdistdash               =55
    repdistlabel              =56
    repribbon                 =57
    repcartoon                =58
    sculpt                    =59
    vfont                     =60
    # in layer0
    shader                    =61

    executive                 =70
    selector                  =71
    editor                    =72
    nag                       =73
    
    export                    =75
    ccmd                      =76
    api                       =77   

    main                      =80  

# This second set, with negative indices
# represent "python-only" subsystems

    parser                    =-1
    cmd                       =-2

class fb_mask:
    output =              0x01 # Python/text output
    results =             0x02
    errors =              0x04
    actions =             0x08
    warnings =            0x10
    details =             0x20
    blather =             0x40
    debugging =           0x80
    everything =          0xFF

