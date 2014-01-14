#A* -------------------------------------------------------------------
#B* This file contains source code for the PyMOL computer program
#C* Copyright (c) Schrodinger, LLC. 
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

if __name__=='pymol.exporting':
    import os
    import thread
    import selector
    import string
    import re
    import copy
    
    import pymol
    import cmd
    from cmd import _cmd,lock,unlock,Shortcut,QuietException
    from chempy import io
    from chempy.sdf import SDF,SDFRec
    from chempy.mol2 import MOL2
    from cmd import _feedback,fb_module,fb_mask, \
                     DEFAULT_ERROR, DEFAULT_SUCCESS, _raising, is_ok, is_error, \
                     is_list, is_dict, is_tuple, loadable
    import traceback

    def copy_image(quiet=1,_self=cmd): # incentive feature / proprietary
        r = DEFAULT_ERROR
        if thread.get_ident() == pymol.glutThread:
            r = _self._copy_image(_self,int(quiet))
        else:
            r = _self.do('cmd._copy_image(quiet=%d,_self=cmd)'%int(quiet))
        if _self._raising(r,_self): raise QuietException         
        return r

    cache_action_dict = {
        'enable'      : 0,
        'disable'     : 1,
        'read_only'   : 2,
        'clear'       : 3,
        'optimize'    : 4,
    }

    cache_action_sc = Shortcut(cache_action_dict.keys())

    def cache(action='optimize', scenes='',state=-1, quiet=1, _self=cmd):
        '''
DESCRIPTION

    "cache" manages storage of precomputed results, such as
    molecular surfaces.

USAGE

    cache action [, scenes [, state ]]

ARGUMENTS

    action = string: enable, disable, read_only, clear, or optimize

    scenes = string: a space-separated list of scene names (default: '')

    state = integer: state index (default: -1)

EXAMPLES

    cache enable
    cache optimize
    cache optimize, F1 F2 F5

NOTES

    "cache optimize" will iterate through the list of scenes provided
    (or all defined scenes), compute any missing surfaces, and store
    them in the cache for later reuse.
    
PYMOL API

    cmd.cache(string action, string scenes, int state, int quiet)

    '''
        
        r = DEFAULT_ERROR
        action = cache_action_dict[cache_action_sc.auto_err(str(action),'action')]
        quiet = int(quiet)
        if action == 0: # enable
            r = _self.set('cache_mode',2,quiet=quiet)
        elif action == 1: # disable
            r =_self.set('cache_mode',0,quiet=quiet)
        elif action == 2: # read_only
            r =_self.set('cache_mode',1,quiet=quiet)
        elif action == 3: # clear
            r =_self._cache_clear(_self=_self)
        elif action == 4: # optimize
            r = DEFAULT_SUCCESS
            _self._cache_mark(_self=_self)
            cur_scene = _self.get('scene_current_name')
            cache_max = int(_self.get('cache_max'))
            if cache_max>0:
                # allow double memory for an optimized cache
                _self.set('cache_max',cache_max*2) 
            scenes = str(scenes)
            scene_list = string.split(scenes)
            cache_mode = int(_self.get('cache_mode'))
            _self.set('cache_mode',2)
            if not len(scene_list):
                scene_list = _self.get_scene_list()
            for scene in scene_list:
                scene = string.strip(scene)
                if not quiet:
                    print " cache: optimizing scene '%s'."%scene
                cmd.scene(scene,animate=0)
                cmd.rebuild()                
                cmd.refresh()
            if len(cur_scene):
                cmd.scene(cur_scene,animate=0)
            else:
                scene_list = _self.get_scene_list()
                if len(scene_list):
                    cmd.scene(scene_list[0],animate=0)
                else:
                    if not quiet:
                        print " cache: no scenes defined -- optimizing current display."
                    cmd.rebuild() 
                    cmd.refresh()
            usage = _self._cache_purge(-1,_self=_self)
            if cache_mode:
                _self.set('cache_mode',cache_mode)
            else:
                _self.set('cache_mode',2) # hmm... could use 1 here instead.
            _self.set('cache_max',cache_max) # restore previous limits
            if not quiet:            
                print " cache: optimization complete (~%0.1f MB)."%(usage*4/1000000.0)
        try:
            _self.lock(_self)
        finally:
            _self.unlock(r,_self)
        if _self._raising(r,_self): raise QuietException         
        return r

    _resn_to_aa =  {
            'ALA' : 'A',
            'CYS' : 'C',
            'ASP' : 'D',
            'GLU' : 'E',
            'PHE' : 'F',
            'GLY' : 'G',
            'HIS' : 'H',
            'ILE' : 'I',
            'LYS' : 'K',
            'LEU' : 'L',
            'MET' : 'M',
            'ASN' : 'N',
            'PRO' : 'P',
            'GLN' : 'Q',
            'ARG' : 'R',
            'SER' : 'S',
            'THR' : 'T',
            'VAL' : 'V',
            'TRP' : 'W',
            'TYR' : 'Y',
            }
    
    def get_fastastr(selection="all", state=-1, quiet=1, _self=cmd):
        dict = { 'seq' : {} }
        # we use (alt '' or alt 'A') because 'guide' picks up 
        # non-canonical structures: eg, 1ejg has residue 22 as a SER and 
        # PRO, which guide will report twice
        _self.iterate("("+selection+") and polymer and name ca and (alt '' or alt 'A')",
                    "seq[model]=seq.get(model,[]);seq[model].append(resn)",space=dict)
        seq = dict['seq']
        result = []
        for obj in _self.get_names("objects",selection='('+selection+')'):
            if seq.has_key(obj):
                cur_seq = map(lambda x:_resn_to_aa.get(x,'?'),seq[obj])
                result.append(">%s"%obj)
                cur_seq = string.join(cur_seq,'')
                while len(cur_seq):
                    if len(cur_seq)>=70:
                        result.append(cur_seq[0:70])
                        cur_seq=cur_seq[70:]
                    else:
                        result.append(cur_seq)
                        break
        result = string.join(result,'\n')
        if len(result):
            result = result + '\n'
        return result

    def get_pdbstr(selection="all", state=-1, ref='', ref_state=-1, quiet=1, _self=cmd):
        '''
DESCRIPTION

    "get_pdbstr" in an API-only function which returns a pdb
    corresponding to the atoms in the selection provided and that are
    present in the indicated state

PYMOL API

    cmd.get_pdbstr(string selection, int state)

NOTES

    "state" is a 1-based state index for the object.

    if state is -1, then current state is used.

    if state is 0, then all states are saved.
    
    '''
        r = DEFAULT_ERROR
        try:
            _self.lock(_self)   
            r = _cmd.get_pdb(_self._COb,str(selection),int(state)-1,0,
                             str(ref),int(ref_state),int(quiet))
        finally:
            _self.unlock(r,_self)
        if _self._raising(r,_self): raise QuietException         
        return r

    def _get_dump_str(obj):
        if is_list(obj):
            list = map(_get_dump_str,obj)
            result = "[ "+string.join(list,",\n")+" ] "
        elif is_dict(obj):
            list = []
            for key in obj.keys():
                list.append( _get_dump_str(key)+" : "+_get_dump_str(obj[key]) )
            result = "{ "+string.join(list,",\n")+" } "
        elif is_tuple(obj):
            list = map(_get_dump_str,obj)
            result = "( "+string.join(list,",\n")+" ) "
        else:
            result = str(obj)
        return result
    
    def get_session(names='', partial=0, quiet=1, compress=-1, cache=-1, _self=cmd):
        session = {}
        r = DEFAULT_SUCCESS
        cache = int(cache)
        compress = int(compress)
        if cache:
            cache_opt = int(_self.get('session_cache_optimize'))
            if cache != 0:
                cache_mode = int(_self.get('cache_mode'))
                if ((cache_mode > 0) and (cache_opt != 0)) or (cache_opt==1):
                    _self.cache('optimize')
        for a in _self._pymol._session_save_tasks:
            if a==None:
                try:
                    _self.lock(_self)
                    r = _cmd.get_session(_self._COb,session,str(names),
                                         int(partial),int(quiet))
                finally:
                    _self.unlock(r,_self)
                try:
                    session['session'] = copy.deepcopy(_self._pymol.session)
                    if cache and hasattr(_self._pymol,'_cache'):
                        session['cache'] = _self._pymol._cache
                except:
                    traceback.print_exc()
            else:
                try:
                    if is_error(apply(a,(session,),{'_self':_self})):
                        r = DEFAULT_ERROR
                except:
                    traceback.print_exc()
                    print "Error: An error occurred when trying to generate session."
                    print "Error: The resulting session file may be incomplete."
        if is_ok(r):
            if(compress<0):
                compress = _self.get_setting_boolean('session_compression')
            if(compress):
                import zlib
                session = zlib.compress(io.pkl.toString(session))
            return session
        elif _self._raising(r,_self):
            raise QuietException                  
        return r
        
    def _unit2px(value, dpi):
        '''API only. Returns pixel units given a string representation in other units'''
        if isinstance(value, str):
            m = re.search(r'[a-z].*', value, re.I)
            if m:
                if dpi <= 0:
                    raise pymol.CmdException('need dpi if units are given')
                value, unit = value[:m.start()], m.group(0).lower()
                upi = {'in': 1.0, 'mm': 25.4, 'cm': 2.54}
                if unit not in upi:
                    raise pymol.CmdException('unknown unit, supported units are: ' +
                            ', '.join(upi))
                value = float(value) * dpi / upi[unit] + 0.5
        return int(value)

    def png(filename, width=0, height=0, dpi=-1.0, ray=0,
            quiet=1, prior=0, format=0, _self=cmd):
        '''
DESCRIPTION

    "png" saves a PNG format image file of the current display.

USAGE

    png filename [, width [, height [, dpi [, ray]]]]

ARGUMENTS

    filename = string: file path to be written
    
    width = integer or string: width in pixels (without units), inches (in)
    or centimeters (cm). If unit suffix is given, dpi argument is required
    as well. If only one of width or height is given, the aspect ratio of
    the viewport is preserved. {default: 0 (current)}

    height = integer or string: height (see width) {default: 0 (current)}

    dpi = float: dots-per-inch {default -1.0 (unspecified)}

    ray = 0 or 1: should ray be run first {default: 0 (no)}

EXAMPLES

    png image.png
    png image.png, dpi=300
    png image.png, 10cm, dpi=300, ray=1

NOTES

    PNG is the only image format supported by PyMOL.

SEE ALSO

    mpng, save
    
PYMOL API

    cmd.png(string filename, int width, int height, float dpi,
            int ray, int quiet)
        '''
        r = DEFAULT_ERROR
        prior = int(prior)
        if prior: 
            # fetch the prior image, without doing any work (fast-path / non-GLUT thread-safe)
            r = _self._png(str(filename),0,0,float(dpi),0,int(quiet),1,
                           int(format),_self)            
            if r != 1: # no prior image available -- revert to default behavior
                if prior < 0: # default is to fall back to actual rendering
                    prior = 0
        if not prior:
            dpi = float(dpi)
            if dpi < 0:
                dpi = cmd.get_setting_float('image_dots_per_inch')
            width = _unit2px(width, dpi)
            height = _unit2px(height, dpi)

            if thread.get_ident() == pymol.glutThread:
                r = _self._png(str(filename),int(width),int(height),float(dpi),
                               int(ray),int(quiet),0,int(format),_self)
            else:
                r = _self._do("cmd._png('''%s''',%d,%d,%1.6f,%d,%d,%d,%d)"%
                              (filename,width,height,dpi,
                               ray,int(quiet),0,int(format)),_self=_self)
        if _self._raising(r,_self): raise QuietException
        return r

    def export_coords(obj,state,_self=cmd): # experimental
        r = DEFAULT_ERROR
        try:
            _self.lock(_self)   
            r = _cmd.export_coords(_self._COb,str(obj),int(state)-1)
        finally:
            _self.unlock(r,_self)
        if _self._raising(r,_self): raise QuietException
        return r

    def multisave(filename, pattern="all", state=-1,
                  append=0, format='', quiet=1, _self=cmd): 
        '''
DESCRIPTION

    "multisave" is an unsupported command.
    
    '''
        r = DEFAULT_ERROR
        filename = _self.exp_path(filename)        
        lc_filename=string.lower(filename)
        if format=='':
            # refactor following if/elif cascade 
            # with a dictionary lookup
            if re.search("\.pdb$|\.ent$",lc_filename):
                format = 'pdb'
            elif re.search("\.pmo$",lc_filename):
                format = 'pmo'
        if format == 'pdb':
            ftype = loadable.pdb
        elif format == 'pmo':
            ftype = loadable.pmo
        else:
            ftype = loadable.pdb # default
        try:
            _self.lock(_self)
            r = _cmd.multisave(_self._COb, str(filename), str(pattern),
                               int(state)-1, int(append), int(ftype),int(quiet))
        finally:
            _self.unlock(r,_self)
        if _self._raising(r,_self): raise QuietException
        return r

    def assign_atom_types( selection, format = "mol2", state=1, quiet=1, _self=cmd):
        r = DEFAULT_ERROR
        try:
            _self.lock(_self)
            # format : mol2/sybyl = 1, macromodel/mmd = 2, global setting atom_type_format = 0
            r = _cmd.assign_atom_types(_self._COb, selection, int(1), int(state-1), quiet)
        finally:
            _self.unlock(r,_self)
        return r

    def save(filename, selection='(all)', state=-1, format='', ref='',
             ref_state=-1, quiet=1, partial=0,_self=cmd):
        '''
DESCRIPTION

    "save" writes content to a file.
    
USAGE

    save filename [, selection [, state [, format ]]]

ARGUMENTS

    filename = string: file path to be written

    selection = string: atoms to save {default: (all)}

    state = integer: state to save {default: -1 (current state)}
    
PYMOL API

    cmd.save(string file, string selection, int state, string format)

NOTES

    The file format is automatically chosen if the extesion is one of
    the supported output formats: pdb, pqr, mol, pkl, mmd, mmod, pov,
    png, pse, aln, obj, mtl, or wrl.

    If the file format is not recognized, then a PDB file is written
    by default.

    For molecular files and where applicable and supported:
    
    * if state = -1 (default), then only the current state is written.

    * if state = 0, then a multi-state output file is written.
    
SEE ALSO

    load, get_model
        '''
        # preprocess selection
        input_selection = selection
        selection = selector.process(input_selection)
        #   
        r = DEFAULT_ERROR
        lc_filename=string.lower(filename)
        if format=='':
            format = 'unknown'
            # refactor following if/elif cascade 
            # with a dictionary lookup
            if re.search("\.pdb$|\.ent$",lc_filename):
                format = 'pdb'
            elif re.search("\.pqr$",lc_filename):
                format = 'pqr'
            elif re.search("\.mol$",lc_filename):
                format = 'mol'
            elif re.search("\.sdf$",lc_filename):
                format = 'sdf'
            elif re.search("\.pkl$",lc_filename):
                format = 'pkl'
            elif re.search("\.pkl$",lc_filename):
                format = 'pkla'
            elif re.search("\.mmd$",lc_filename):
                format = 'mmod'
            elif re.search("\.out$",lc_filename):
                format = 'mmod'
            elif re.search("\.dat$",lc_filename):
                format = 'mmod'
            elif re.search("\.mmod$",lc_filename):
                format = 'mmod'
            elif re.search("\.pmo$",lc_filename):
                format = 'pmo'
            elif re.search("\.pov$",lc_filename):
                format = 'pov'
            elif re.search("\.png$",lc_filename):
                format = 'png'
            elif re.search("\.pse$|\.psw$",lc_filename):
                format = 'pse'
            elif re.search("\.aln$",lc_filename):
                format = 'aln'
            elif re.search("\.fasta$",lc_filename):
                format = 'fasta'
            elif re.search("\.obj$",lc_filename):
                format = 'obj'
            elif re.search("\.mtl$",lc_filename):
                format = 'mtl'
            elif re.search("\.wrl$",lc_filename):
                format = 'wrl'
            elif re.search("\.idtf$",lc_filename):
                format = 'idtf'
            elif re.search("\.mol2$",lc_filename):
                format = 'mol2'
            else:
                format = str(format)
        if format=='unknown':
            if not quiet:
                print " Save-Warning: Unrecognized file type -- defaulting to PDB format."
            format='pdb'
        filename = _self.exp_path(filename)
        if format=='pdb': # standard PDB file 
            f=open(filename,"w")
            if f:
                st = ''
                try:
                    _self.lock(_self)
                    st = _cmd.get_pdb(_self._COb,"("+str(selection)+")",int(state)-1,0,
                                      str(ref),int(ref_state)-1,int(quiet))
                    if st != None:
                        r = DEFAULT_SUCCESS                    
                finally:
                    _self.unlock(r,_self=_self)
                f.write(st)
                f.close()
                if not quiet:
                    print " Save: wrote \""+filename+"\"."
        elif format=='aln':
            st = ''
            try:
                _self.lock(_self)
                st = _cmd.get_seq_align_str(_self._COb,str(selection),int(state)-1,0,int(quiet))
                if st != None:
                    r = DEFAULT_SUCCESS
            finally:
                _self.unlock(r,_self)
            if st!=None:
                f=open(filename,"w")
                f.write(st)
                f.close()
                if not quiet:
                    print " Save: wrote \""+filename+"\"."
            else:
                r = DEFAULT_ERROR
        elif format=='fasta':
            st = _self.get_fastastr(selection)
            if st != None:
                r = DEFAULT_SUCCESS
                f=open(filename,"w")
                f.write(st)
                f.close()
                if not quiet:
                    print " Save: wrote \""+filename+"\"."
            else:
                r = DEFAULT_ERROR
        elif format=='pqr': # PQR (modified PDB file)
            f=open(filename,"w")
            if f:
                st = ''
                try:
                    _self.lock(_self)
                    st = _cmd.get_pdb(_self._COb,"("+str(selection)+")",int(state)-1,1,
                                      str(ref),int(ref_state)-1,int(quiet))
                    if st != None:
                        r = DEFAULT_SUCCESS
                finally:
                    _self.unlock(r,_self)
                f.write(st)
                f.close()
                if not quiet:
                    print " Save: wrote \""+filename+"\"."
        elif format=='pkl': # python binary
            io.pkl.toFile(_self.get_model(selection,state,ref,ref_state),filename)
            r = DEFAULT_SUCCESS
            if not quiet:
                print " Save: wrote \""+filename+"\"."
        elif format=='pkla': # ascii override
            io.pkl.toFile(_self.get_model(selection,state,ref,ref_state),filename,bin=0)
            r = DEFAULT_SUCCESS
            if not quiet:
                print " Save: wrote \""+filename+"\"."
        elif format=='pse': # PyMOL session
            filename = filename.replace("\\","/") # always use unix-like path separators	
            _self.set("session_file",filename,quiet=1)
            if '(' in input_selection: # ignore selections 
                input_selection=''
            if not quiet:
                print " Save: Please wait -- writing session file..."
            io.pkl.toFile(_self.get_session(str(input_selection),int(partial),int(quiet)),filename)
            r = DEFAULT_SUCCESS
            if not quiet:
                print " Save: wrote \""+filename+"\"."
        elif format=='mmod': # macromodel
            io.mmd.toFile(_self.get_model(selection,state,ref,ref_state),filename)
            r = DEFAULT_SUCCESS
            if not quiet:
                print " Save: wrote \""+filename+"\"."
        elif format=='sdf':
            state = int(state)
            sdf = SDF(filename,'w')
            for i, (selection, state) in enumerate(
                    pymol.selecting.objsele_state_iter(selection, state)):
                rec = SDFRec(io.mol.toList(_self.get_model(selection,state,ref,ref_state))
                             + ["$$$$\n"])
                sdf.write(rec)
            r = DEFAULT_SUCCESS
            if not quiet:
                print " Save: wrote %d states to \"%s\"." % (i + 1, filename)
        elif format=='mol':
            io.mol.toFile(_self.get_model(selection,state,ref,ref_state),filename)
            r = DEFAULT_SUCCESS
            if not quiet:
                print " Save: wrote \""+filename+"\"."
        elif format=="mol2":
            state = int(state)
            recList = []
            entrycount = 0
            for osele, ostate in pymol.selecting.objsele_state_iter(selection, state):
                    assign_atom_types(osele, "mol2", ostate, 1, _self)
                    recList.extend(io.mol2.toList(_self.get_model(osele,
                        ostate, ref, ref_state), selection=osele, state=ostate))
                    entrycount += 1
            m = MOL2(cmd=cmd)
            m.strToFile(recList,filename)
            r = DEFAULT_SUCCESS
            if not quiet:
                print ' Save: wrote %d entries to "%s".' % (entrycount, filename)
        elif format=='png':
            r = _self.png(filename,quiet=quiet)
        # refactor below to lift repeated code
        elif format=='pov':
            tup = _self.get_povray()
            f=open(filename,"w")
            f.write(tup[0])
            f.write(tup[1])
            f.flush()
            f.close()
            if not quiet:
                print " Save: wrote \""+filename+"\"."
            r = DEFAULT_SUCCESS
        elif format=='obj':
            tup = _self.get_mtl_obj()
            f=open(filename,"w")
            f.write(tup[1])
            f.flush()
            f.close()
            r = DEFAULT_SUCCESS
        elif format=='mtl':
            tup = _self.get_mtl_obj()
            f=open(filename,"w")
            f.write(tup[0])
            f.flush()
            f.close()
            r = DEFAULT_SUCCESS
        elif format=='wrl':
            txt = _self.get_vrml()
            f=open(filename,"w")
            f.write(txt)
            f.flush()
            f.close()
            if not quiet:
                print " Save: wrote \""+filename+"\"."
            r = DEFAULT_SUCCESS
        elif format=='idtf':
            tup = _self.get_idtf()
            f=open(filename,"w")
            f.write(tup[0]);
            f.write(tup[1]);
            f.flush()
            f.close()
            if not quiet:
                fov = float(cmd.get("field_of_view"))
                dist = cmd.get_view()[11]
                print " 3Daac=%3.1f, 3Droll=0, 3Dc2c=0 0 1, 3Droo=%1.2f, 3Dcoo=0 0 %1.2f"%(fov,-dist,dist)
                print " Save: wrote \""+filename+"\"."
            r = DEFAULT_SUCCESS
            
        if _self._raising(r,_self): raise QuietException
        return r

