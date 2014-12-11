'''
Volume color ramp utilities
'''

import math
import cmd

_volume_windows = {}

def peak(v, c, a=0.2, d=0.2):
    return [v - d, c, 0., v, c, a, v + d, c, 0.]

namedramps = {
    '2fofc': [
         1.,  "blue",   .0,
         1.,  "blue",   .2,
         1.4, "blue",   .0,
    ],
    'fofc': [
        -3.5, "red",   .0,
        -3.,  "red",   .2,
        -2.9, "red",   .0,
         2.9, "green", .0,
         3.,  "green", .2,
         3.5, "green", .0,
    ],
    'esp': [
        -1.2, "red",  .0,
        -1.,  "red",  .2,
        -1.,  "0xff9999", .0,
         1.,  "0x9999ff", .0,
         1.,  "blue", .2,
         1.2, "blue", .0,
    ],
    'rainbow': [
        1.0, 'blue', 0.1,
        1.5, 'cyan', 0.1,
        2.0, 'green', 0.1,
        2.5, 'yellow', 0.1,
        3.0, 'orange', 0.1,
        3.5, 'red', 0.1,
    ],
    'rainbow2':
        peak(1.0, 'blue') +
        peak(1.5, 'cyan') +
        peak(2.0, 'green') +
        peak(2.5, 'yellow') +
        peak(3.0, 'orange') +
        peak(3.5, 'red'),
}

def volume_ramp_new(name, ramp):
    '''
DESCRIPTION

    Register a named volume ramp which can be used as a preset
    when creating or coloring volumes. The name will appear in the
    internal menu at "A > volume" and "C".

ARGUMENTS

    name = string: name of the new ramp

    ramp = list: space delimited list of value, color, alpha

EXAMPLE

    volume_ramp_new pink1sigma, \\
       0.9 violet 0.0 \\
       1.0 magenta 0.3 \\
       1.5 pink 0.0

SEE ALSO

    volume, volume_color
    '''
    if isinstance(ramp, basestring):
        ramp = ramp.split()
    namedramps[name] = ramp


def get_volume_color(name, quiet=1, _self=cmd):
    '''
DESCRIPTION

    Get the volume color ramp of a volume object.
    '''
    quiet = int(quiet)

    r = _self.DEFAULT_ERROR
    with _self.lockcm:
        r = _self._cmd.get_volume_ramp(_self._COb, name)

    if isinstance(r, list):
        if not quiet:
            import random
            rname = 'ramp%03d' % random.randint(0, 999)
            print '### cut below here and paste into script ###'
            print 'cmd.volume_ramp_new(%s, [\\' % repr(rname)
            for i in range(0, len(r), 5):
                print '    %6.2f, %.2f, %.2f, %.2f, %.2f, \\' % tuple(r[i:i+5])
            print '    ])'
            print '### cut above here and paste into script ###'
        elif quiet < 0:
            print 'volume_color %s, ' % (name),
            for i in range(0, len(r), 5):
                print '\\\n    %6.2f %.2f %.2f %.2f %.2f' % tuple(r[i:i+5]),
            print ''

    return r

def volume_color(name, ramp='', quiet=1, _guiupdate=True, _self=cmd):
    '''
DESCRIPTION

    Set or get the volume colors.

ARGUMENTS

    name = str: volume object name

    ramp = str, list or empty: named ramp, space delimited string or list
    with (x, color, alpha, ...) or (x, r, g, b, alpha, ...) values. If empty, get
    the current volume colors.

EXAMPLE

    fetch 1a00, map, type=2fofc
    volume vol, map
    volume_color vol, .8 cyan 0. 1. blue .3 2. yellow .3
    '''
    quiet = int(quiet)

    if not ramp:
        return get_volume_color(name, quiet, _self)

    if isinstance(ramp, str) and ramp in namedramps:
        ramp = namedramps[ramp]

    ramplist = ramp_expand(ramp)

    with _self.lockcm:
        r = cmd._cmd.set_volume_ramp(_self._COb, name, ramplist)

    if _guiupdate and name in _volume_windows:
        from pymol import gui
        def func():
            import Tkinter
            try:
                panel = _volume_windows[name].panel
                panel.set_flat(ramplist)
            except (LookupError, Tkinter.TclError):
                pass
        app = gui.get_pmgapp()
        app.execute(func)

    return r

def volume_panel(name, quiet=1, _self=cmd):
    '''
DESCRIPTION

    Open an interactive volume ramp panel

ARGUMENTS

    name = str: name of volume object
    '''
    from pymol import gui
    from pmg_tk import volume
    import Tkinter

    app = gui.get_pmgapp()
    def func():
        try:
            window = _volume_windows[name]
            window.lift()
        except (LookupError, Tkinter.TclError):
            window = Tkinter.Toplevel(app.root)
            window.title('Volume Panel for "%s"' % name)
            window.panel = volume.VolumePanel(window, name, _self=cmd)
            window.panel.pack()
            _volume_windows[name] = window
    app.execute(func)

### utility functions

def ramp_to_colors(ramp, vmin=None, vmax=None, ncolors=360):
    '''
    Get the interpolated color array for the given ramp and data range
    '''
    ramp = ramp_expand(ramp)

    if vmin is None: vmin = ramp[0]
    if vmax is None: vmax = ramp[-5]

    colors = [(0.0, 0.0, 0.0, 0.0)] * ncolors
    upper = None

    for i in range(len(ramp) / 5):
        v = ramp[i * 5]
        lower = upper
        upper = int(ncolors * (v - vmin) / (vmax - vmin))

        if i == 0:
            continue

        mixcincr = 1.0 / (upper - lower)
        mixc = 1.0 + mixcincr

        for j in xrange(lower, upper):
            mixc -= mixcincr
            if j < 0 or j >= ncolors:
                continue
            colors[j] = [
                    ramp[i * 5 - 1 + k + 1] * mixc + \
                    ramp[i * 5     + k + 1] * (1.0 - mixc)
                    for k in xrange(4)]

    return colors

def ramp_expand(ramp, _self=cmd):
    '''
    Takes a list or space separated string of (v,r,g,b,a) or (v,colorname,a)
    specs and flattens it to a list of floats.

    >>> print ramp_expand('1.2 blue 0.3 4.5 red .7')
    [1.2, 0.0, 0.0, 1.0, 0.3, 4.5, 1.0, 0.0, 0.0, 0.7]

    @rtype: list
    '''
    if isinstance(ramp, str):
        if ramp[:1] in '([':
            ramp = _self.safe_eval(ramp)
        else:
            ramp = ramp.split()
    it = flatiter(ramp)
    ramp = []
    for s in it:
        try:
            c = it.next()
            try:
                r = float(c)
                if r > 1.0:
                    # support for numeric color index except 0+1 (white+black)
                    raise ValueError
            except ValueError:
                r, g, b = _self.get_color_tuple(c)
            else:
                g, b = float(it.next()), float(it.next())
            a = float(it.next())
        except StopIteration:
            raise ValueError("malformed color ramp")
        ramp.extend([float(s), r, g, b, a])
    return ramp

def flatiter(x):
    '''
    Flat iterator over nested list-like structures (does not flatten strings).

    >>> list(flatiter([1,2,(3,4),5,("foo", (7,8), "bar"),10]))
    [1, 2, 3, 4, 5, 'foo', 7, 8, 'bar', 10]

    Details: Flattens all elements with an __iter__ method. Sequences with
    __len__ and __getitem__ are also iterable, but are not flattened by this
    function. Strings don't have an __iter__ method in Python 2, but in Python
    3 they have! So this is not Python 3 compatible without modification.

    @type x: iterable
    @rtype: generator
    '''
    x = [iter(x)]
    while x:
        try:
            e = x[-1].next()
        except:
            x.pop()
            continue
        if hasattr(e, '__iter__'):
            x.append(iter(e))
            continue
        yield e

def flatlist(x):
    '''
    Same as list(flatiter(x)) but faster.
    '''
    r = []
    def loop(x):
        for e in x:
            if hasattr(e, '__iter__'):
                loop(e)
            else:
                r.append(e)
    loop(x)
    return r
