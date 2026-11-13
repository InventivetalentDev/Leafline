# Builds /Game/Leafline/Materials/M_TreeBark2 from the HLSL files beside this script. It is the body of an unreal-mcp
# ProgrammaticToolset.execute_tool_script call (the main session pastes it; agents never write .uasset files).
# Re-running it on an existing material only replaces the Custom nodes' Code (MODE='code'): never delete and recreate
# an asset of the same name in one editor session (CLAUDE.md).
import json
MODE = 'build'  # 'build' creates the material; 'code' updates the Code of the three Custom nodes and recompiles
DIR = 'D:/UnrealProjects/AntTreeGame/Tools/materials/'
MAT = '/Game/Leafline/Materials/M_TreeBark2.M_TreeBark2'
MPC = '/Game/Leafline/Materials/MPC_View.MPC_View'
M = 'editor_toolset.toolsets.material.MaterialTools.'
O = 'editor_toolset.toolsets.object.ObjectTools.'
A = 'editor_toolset.toolsets.asset.AssetTools.'

def t(name, args):
    return execute_tool(name, json.dumps(args))

def rp(e):
    return e['refPath'] if isinstance(e, dict) else str(e)

def code(name):
    with open(DIR + name, 'r') as f:
        return f.read()

def sp(path, vals):
    return t(O + 'set_properties', {'instance': {'refPath': path}, 'values': json.dumps(vals)})['returnValue']

def add(cls, x, y, props=None):
    e = rp(t(M + 'add_expression', {'material_or_function': {'refPath': MAT}, 'expression_class': {'refPath': '/Script/Engine.MaterialExpression' + cls}, 'x': x, 'y': y})['returnValue'])
    if props:
        sp(e, props)
    return e

def gp(path, prop):
    got = t(O + 'get_properties', {'instance': {'refPath': path}, 'properties': [prop]})['returnValue']
    return (json.loads(got) if isinstance(got, str) else got)[prop]

def key(d, base):
    for k in (base[0].lower() + base[1:], base):
        if k in d:
            return k
    return base

def grow(path, prop, items):
    # A new Custom node already has one (unnamed) input. Arrays of structs only take when the existing elements are
    # sent back exactly as read and one element is appended per call ("elements changed alongside the size change").
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

def custom(x, y, desc, file, inputs, outtype, extra=None):
    e = add('Custom', x, y, {'Code': code(file), 'OutputType': outtype, 'Description': desc})
    grow(e, 'Inputs', [{'InputName': n} for n in inputs])
    if extra:
        grow(e, 'AdditionalOutputs', [{'OutputName': n, 'OutputType': ty} for (n, ty) in extra])
    return e

def con(a, ao, b, bi):
    t(M + 'connect_expressions', {'from_expression': {'refPath': a}, 'from_output_name': ao, 'to_expression': {'refPath': b}, 'to_input_name': bi})

def mpc(name, x, y):
    return add('CollectionParameter', x, y, {'Collection': MPC, 'ParameterName': name})

def build():
    t(M + 'create_material', {'folder_path': '/Game/Leafline/Materials', 'asset_name': 'M_TreeBark2'})
    # The material was created on 13 Nov in steps (this script's first run stopped at the inputs); this is the graph it has.
    X = -1800
    uv1 = add('TextureCoordinate', X, -600, {'CoordinateIndex': 1})
    uv2 = add('TextureCoordinate', X, -480, {'CoordinateIndex': 2})
    vc = add('VertexColor', X, -360)
    wp = add('WorldPosition', X, -240)
    nrm = add('VertexNormalWS', X, -120)
    tan = add('VertexTangentWS', X, 0)
    cam = mpc('CamPos', X, 120)
    ant = mpc('AntFocus', X, 240)
    on = mpc('CutawayOn', X, 360)
    rad = mpc('CutawayRadius', X, 480)
    band = mpc('CutawayBand', X, 600)
    taper = mpc('CutawayTaper', X, 720)
    na = mpc('NearFadeStart', X, 840)
    nb = mpc('NearFadeEnd', X, 960)
    bark = custom(-1100, -300, 'Bark', 'bark2_bark.hlsl', ['UV1', 'UV2', 'VC', 'WP', 'N', 'T', 'Cam'], 'CMOT_Float3',
                  [('Rough', 'CMOT_Float1'), ('NormalW', 'CMOT_Float3'), ('AO', 'CMOT_Float1')])
    cut = custom(-1100, 500, 'Cut', 'bark2_cut.hlsl', ['WP', 'Cam', 'Ant', 'R', 'On', 'Band', 'Taper', 'NearA', 'NearB', 'VC'], 'CMOT_Float1')
    keep = custom(-1100, 900, 'Keep', 'bark2_keep.hlsl', ['VC'], 'CMOT_Float1')
    sr = add('ShadowReplace', -700, 650)
    for (src, pin) in [(uv1, 'UV1'), (uv2, 'UV2'), (vc, 'VC'), (wp, 'WP'), (nrm, 'N'), (tan, 'T'), (cam, 'Cam')]:
        con(src, '', bark, pin)
    for (src, pin) in [(wp, 'WP'), (cam, 'Cam'), (ant, 'Ant'), (rad, 'R'), (on, 'On'), (band, 'Band'), (taper, 'Taper'), (na, 'NearA'), (nb, 'NearB')]:
        con(src, '', cut, pin)
    # The vertex colour's main output is RGB only: the buried flag comes from its A output.
    con(vc, 'A', cut, 'VC')
    con(vc, 'A', keep, 'VC')
    con(cut, '', sr, 'Default')
    con(keep, '', sr, 'Shadow')
    outs = t(M + 'get_expression_output_names', {'expression': {'refPath': bark}})['returnValue']
    def out(e, name, prop):
        t(M + 'connect_to_output', {'expression': {'refPath': e}, 'output_name': name, 'material_property': prop})
    out(bark, '', 'MP_BaseColor')
    out(bark, 'Rough', 'MP_Roughness')
    out(bark, 'NormalW', 'MP_Normal')
    out(bark, 'AO', 'MP_AmbientOcclusion')
    out(sr, '', 'MP_OpacityMask')
    sp(MAT, {'BlendMode': 'BLEND_Masked', 'DitherOpacityMask': True, 'OpacityMaskClipValue': 0.5, 'bTangentSpaceNormal': False})
    return {'bark': bark, 'cut': cut, 'keep': keep, 'bark_outputs': outs}

def update_code():
    exprs = [rp(e) for e in t(M + 'get_expressions', {'material_or_function': {'refPath': MAT}})['returnValue']]
    done = []
    for e in exprs:
        if 'MaterialExpressionCustom' not in e:
            continue
        got = t(O + 'get_properties', {'instance': {'refPath': e}, 'properties': ['Description']})['returnValue']
        desc = (json.loads(got) if isinstance(got, str) else got).get('Description')
        f = {'Bark': 'bark2_bark.hlsl', 'Cut': 'bark2_cut.hlsl', 'Keep': 'bark2_keep.hlsl'}.get(desc)
        if f:
            sp(e, {'Code': code(f)})
            done.append(desc)
    # Setting only Code did not always mark the package dirty (13 Nov): nudge a material property so it saves.
    sp(MAT, {'OpacityMaskClipValue': 0.5001})
    sp(MAT, {'OpacityMaskClipValue': 0.5})
    return {'updated': done}

def run():
    res = build() if MODE == 'build' else update_code()
    errs = []
    try:
        t(M + 'recompile', {'material_or_function': {'refPath': MAT}})
    except Exception as ex:
        errs.append(str(ex)[:1500])
    t(A + 'save_assets', {'asset_paths': []})
    res['errors'] = errs
    return res
