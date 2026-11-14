# Builds /Game/Leafline/Materials/M_Ground2 (floor pass, 14 Nov, docs/briefs/floor-from-above.md steps 2 and 4a) as a
# DUPLICATE of M_Ground, so the near look stays M_Ground's exactly, and puts one Custom node, "GroundFar"
# (Tools/materials/ground2.hlsl), between M_Ground's own Base Colour / Roughness / Normal and those outputs. Specular,
# AO and everything else stay connected as they are. The game loads M_Ground2 by path and falls back to M_Ground
# (ATreeActor::PickGround, Leaf.Ground 0|1); the tuning scalars are set on its dynamic instance (Leaf.Ground 1 Name Value).
# It is the body of an unreal-mcp ProgrammaticToolset.execute_tool_script call (the main session pastes it; agents never
# write .uasset files). MODE='build' once, under the new name only (never delete and recreate an asset of the same name in
# one editor session); MODE='code' replaces the GroundFar node's Code on an existing M_Ground2; MODE='probe' changes
# nothing and returns what feeds M_Ground's outputs (for SOURCES below if the automatic lookup fails).
import json
MODE = 'build'
HLSL = 'D:/UnrealProjects/leafline-floorfar/Tools/materials/ground2.hlsl'
SRC = '/Game/Leafline/Materials/M_Ground.M_Ground'
MAT = '/Game/Leafline/Materials/M_Ground2.M_Ground2'
# Fill from a dump if the lookup fails: material property -> (expression path in M_Ground2, output name ('' = first)).
# e.g. 'MP_BaseColor': ('/Game/Leafline/Materials/M_Ground2.M_Ground2:MaterialExpressionLerp_2', '')
SOURCES = {'MP_BaseColor': None, 'MP_Roughness': None, 'MP_Normal': None}
M = 'editor_toolset.toolsets.material.MaterialTools.'
O = 'editor_toolset.toolsets.object.ObjectTools.'
A = 'editor_toolset.toolsets.asset.AssetTools.'
PROPS = {'MP_BaseColor': 'BaseColor', 'MP_Roughness': 'Roughness', 'MP_Normal': 'Normal'}

def t(name, args):
    return execute_tool(name, json.dumps(args))

def rp(e):
    return e['refPath'] if isinstance(e, dict) else str(e)

def node_code(n):
    with open(HLSL, 'r') as f:
        src = f.read()
    b = src.index('---- BEGIN NODE %d' % n)
    b = src.index('\n', b) + 1
    e = src.index('// ---- END NODE %d' % n, b)
    return src[b:e]

def sp(path, vals):
    return t(O + 'set_properties', {'instance': {'refPath': path}, 'values': json.dumps(vals)})['returnValue']

def gp(path, prop):
    got = t(O + 'get_properties', {'instance': {'refPath': path}, 'properties': [prop]})['returnValue']
    return (json.loads(got) if isinstance(got, str) else got)[prop]

def add(cls, x, y, props=None):
    e = rp(t(M + 'add_expression', {'material_or_function': {'refPath': MAT}, 'expression_class': {'refPath': '/Script/Engine.MaterialExpression' + cls}, 'x': x, 'y': y})['returnValue'])
    if props:
        sp(e, props)
    return e

def key(d, base):
    for k in (base[0].lower() + base[1:], base):
        if k in d:
            return k
    return base

def grow(path, prop, items):
    # Same as build_bark2_mcp.py: struct arrays only take when existing elements go back exactly as read, one per call.
    cur = gp(path, prop)
    changed = False
    for i in range(min(len(cur), len(items))):
        for kk, vv in items[i].items():
            k = key(cur[i], kk)
            if cur[i].get(k) != vv:
                cur[i][k] = vv
                changed = True
    if changed:
        sp(path, {prop: cur})
        cur = gp(path, prop)
    while len(cur) < len(items):
        it = items[len(cur)]
        el = dict(cur[-1]) if cur else dict(it)
        for kk, vv in it.items():
            el[key(el, kk)] = vv
        sp(path, {prop: cur + [el]})
        cur = gp(path, prop)

def custom(x, y, desc, n, inputs, outtype, extra=None):
    e = add('Custom', x, y, {'Code': node_code(n), 'OutputType': outtype, 'Description': desc})
    grow(e, 'Inputs', [{'InputName': i} for i in inputs])
    if extra:
        grow(e, 'AdditionalOutputs', [{'OutputName': nm, 'OutputType': ty} for (nm, ty) in extra])
    return e

def con(a, ao, b, bi):
    t(M + 'connect_expressions', {'from_expression': {'refPath': a}, 'from_output_name': ao, 'to_expression': {'refPath': b}, 'to_input_name': bi})

def out(e, name, prop):
    t(M + 'connect_to_output', {'expression': {'refPath': e}, 'output_name': name, 'material_property': prop})

def dup():
    # As in build_ant2_mcp.py: the duplicate tool's argument names, tried in turn.
    tries = [
        {'source_asset_path': SRC, 'destination_path': '/Game/Leafline/Materials/M_Ground2'},
        {'source_path': SRC, 'destination_path': '/Game/Leafline/Materials/M_Ground2'},
        {'asset_path': SRC, 'new_path': '/Game/Leafline/Materials/M_Ground2'},
        {'source': SRC, 'destination': '/Game/Leafline/Materials/M_Ground2'},
    ]
    last = None
    for a in tries:
        try:
            return t(A + 'duplicate_asset', a)
        except Exception as ex:
            last = str(ex)[:600]
    raise Exception('duplicate_asset failed: ' + str(last))

def find_ref(v):
    # The expression path anywhere in a property value read back from an FExpressionInput.
    if isinstance(v, str):
        return v if 'MaterialExpression' in v else None
    if isinstance(v, dict):
        for k in ('Expression', 'expression'):
            if k in v:
                r = find_ref(v[k])
                if r:
                    return r
        for vv in v.values():
            r = find_ref(vv)
            if r:
                return r
    if isinstance(v, list):
        for vv in v:
            r = find_ref(vv)
            if r:
                return r
    return None

def field(v, name, default=None):
    if isinstance(v, dict):
        for k in (name, name[0].lower() + name[1:]):
            if k in v:
                return v[k]
    return default

def read_inputs(mat_path, mat_name):
    # The material outputs' inputs live on the material (older engines) or its editor-only data subobject (UE5); the
    # subobject keeps the source's name in a duplicate, so try both names.
    found, tried = {}, []
    hosts = [mat_path, mat_path + ':' + mat_name + 'EditorOnlyData', mat_path + ':M_GroundEditorOnlyData', mat_path + ':M_Ground2EditorOnlyData']
    for prop, pname in PROPS.items():
        for h in hosts:
            try:
                v = gp(h, pname)
            except Exception as ex:
                tried.append('%s %s: %s' % (h, pname, str(ex)[:160]))
                continue
            tried.append('%s %s -> %s' % (h, pname, json.dumps(v)[:300]))
            ref = find_ref(v)
            if ref:
                found[prop] = (ref, v)
                break
            if v is not None:
                found.setdefault(prop, (None, v))
    return found, tried

def resolve_sources():
    res, notes = {}, []
    found, tried = read_inputs(MAT, 'M_Ground2')
    for prop in PROPS:
        if SOURCES.get(prop):
            res[prop] = SOURCES[prop]
            continue
        ref, v = found.get(prop, (None, None))
        if not ref:
            res[prop] = None   # unconnected (or not found): a constant stands in
            notes.append('%s: no expression found (%s)' % (prop, json.dumps(v)[:200] if v is not None else 'unreadable'))
            continue
        ref = ref.replace('M_Ground.M_Ground:', 'M_Ground2.M_Ground2:')
        idx = field(v, 'OutputIndex', 0) or 0
        if field(v, 'Mask', 0):
            chans = [c for c in ('R', 'G', 'B', 'A') if field(v, 'Mask' + c, 0)]
            raise Exception('%s is fed through a channel mask %s from %s: set SOURCES by hand' % (prop, chans, ref))
        if idx != 0:
            raise Exception('%s comes from output %d of %s: set SOURCES[%r] = (path, output name) by hand' % (prop, idx, ref, prop))
        res[prop] = (ref, '')
    return res, notes, tried

def build():
    log = []
    dup()
    src, notes, tried = resolve_sources()
    log += notes
    if not src.get('MP_BaseColor'):
        return {'error': 'Base Colour source not found; M_Ground2 exists but is unchanged. Set SOURCES and run MODE=wire.', 'tried': tried}
    return wire(src, log)

def wire(src=None, log=None):
    log = log or []
    if src is None:
        src, notes, tried = resolve_sources()
        log += notes
    X = -2200
    wp = add('WorldPosition', X, 600)          # absolute world position (default)
    cam = add('CameraPositionWS', X, 700)
    params = [('FarStart', 1800.0), ('FarEnd', 5000.0), ('Fleck', 110.0), ('FarDark', 1.0), ('Undulate', 0.3), ('UndSize', 450.0)]
    pe = []
    for k, (nm, dv) in enumerate(params):
        pe.append(add('ScalarParameter', X, 820 + 90 * k, {'ParameterName': nm, 'DefaultValue': dv}))
    node = custom(-1500, 400, 'GroundFar', 1,
                  ['NearCol', 'NearRough', 'NearN', 'WP', 'Cam'] + [nm for (nm, _) in params],
                  'CMOT_Float3', [('Rough', 'CMOT_Float1'), ('Nrm', 'CMOT_Float3')])
    # Near inputs: what fed M_Ground's outputs, or constants where nothing did.
    con(src['MP_BaseColor'][0], src['MP_BaseColor'][1], node, 'NearCol')
    if src.get('MP_Roughness'):
        con(src['MP_Roughness'][0], src['MP_Roughness'][1], node, 'NearRough')
    else:
        con(add('Constant', X, 400, {'R': 0.9}), '', node, 'NearRough')
        log.append('roughness: constant 0.9 (M_Ground had none connected)')
    if src.get('MP_Normal'):
        con(src['MP_Normal'][0], src['MP_Normal'][1], node, 'NearN')
    else:
        up = add('Constant3Vector', X, 500)
        for v in ({'r': 0.0, 'g': 0.0, 'b': 1.0, 'a': 0.0}, {'R': 0.0, 'G': 0.0, 'B': 1.0, 'A': 0.0}, '(R=0.0,G=0.0,B=1.0,A=0.0)'):
            try:
                sp(up, {'Constant': v})
                break
            except Exception:
                pass
        con(up, '', node, 'NearN')
        log.append('normal: constant (0,0,1) (M_Ground had none connected)')
    con(wp, '', node, 'WP')
    con(cam, '', node, 'Cam')
    for e, (nm, _) in zip(pe, params):
        con(e, '', node, nm)
    # Connecting a new expression to an output replaces the old link; the old chains stay and feed the node.
    out(node, '', 'MP_BaseColor')
    out(node, 'Rough', 'MP_Roughness')
    out(node, 'Nrm', 'MP_Normal')
    log.append('sources: ' + json.dumps(src))
    return {'node': node, 'log': log}

def update_code():
    exprs = [rp(e) for e in t(M + 'get_expressions', {'material_or_function': {'refPath': MAT}})['returnValue']]
    done = []
    for e in exprs:
        if 'MaterialExpressionCustom' not in e:
            continue
        got = t(O + 'get_properties', {'instance': {'refPath': e}, 'properties': ['Description']})['returnValue']
        desc = (json.loads(got) if isinstance(got, str) else got).get('Description')
        if desc == 'GroundFar':
            sp(e, {'Code': node_code(1)})
            done.append(desc)
    # Setting only Code did not always mark the package dirty (13 Nov): nudge a material property so it saves.
    sp(MAT, {'OpacityMaskClipValue': 0.3334})
    sp(MAT, {'OpacityMaskClipValue': 0.3333})
    return {'updated': done}

def run():
    if MODE == 'probe':
        found, tried = read_inputs(SRC, 'M_Ground')
        return {'found': {k: v[0] for k, v in found.items()}, 'tried': tried}
    res = build() if MODE == 'build' else (wire() if MODE == 'wire' else update_code())
    if 'error' in res:
        return res
    errs = []
    try:
        t(M + 'recompile', {'material_or_function': {'refPath': MAT}})
    except Exception as ex:
        errs.append(str(ex)[:1500])
    t(A + 'save_assets', {'asset_paths': []})
    res['errors'] = errs
    return res
