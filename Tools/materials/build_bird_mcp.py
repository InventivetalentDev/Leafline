# Builds /Game/Leafline/Materials/M_Bird from Tools/materials/bird.hlsl (two Custom nodes between BEGIN/END markers).
# It is the body of an unreal-mcp ProgrammaticToolset.execute_tool_script call (the main session pastes it; agents never
# write .uasset files). MODE='code' only replaces the two nodes' Code on an existing M_Bird: never delete and recreate an
# asset of the same name in one editor session (CLAUDE.md).
import json
MODE = 'build'  # 'build' creates the material; 'code' updates the Code of both Custom nodes and recompiles
HLSL = 'D:/UnrealProjects/AntTreeGame/Tools/materials/bird.hlsl'
MAT = '/Game/Leafline/Materials/M_Bird.M_Bird'
M = 'editor_toolset.toolsets.material.MaterialTools.'
O = 'editor_toolset.toolsets.object.ObjectTools.'
A = 'editor_toolset.toolsets.asset.AssetTools.'

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

def build():
    t(M + 'create_material', {'folder_path': '/Game/Leafline/Materials', 'asset_name': 'M_Bird'})
    X = -1200
    uv0 = add('TextureCoordinate', X, -300, {'CoordinateIndex': 0})
    uv1 = add('TextureCoordinate', X, -180, {'CoordinateIndex': 1})
    lp = add('PreSkinnedLocalPosition', X, 200)
    ph = add('ScalarParameter', X, 320, {'ParameterName': 'FlapPhase', 'DefaultValue': 0.0})
    am = add('ScalarParameter', X, 440, {'ParameterName': 'FlapAmount', 'DefaultValue': 0.0})
    colr = custom(-700, -250, 'BirdColour', 1, ['UV0', 'UV1'], 'CMOT_Float3', [('Rough', 'CMOT_Float1')])
    flap = custom(-700, 300, 'BirdFlap', 2, ['LP', 'UV0', 'FlapPhase', 'FlapAmount'], 'CMOT_Float3')
    xf = add('Transform', -350, 300, {'TransformSourceType': 'TRANSFORMSOURCE_Local', 'TransformType': 'TRANSFORM_World'})
    con(uv0, '', colr, 'UV0')
    con(uv1, '', colr, 'UV1')
    con(lp, '', flap, 'LP')
    con(uv0, '', flap, 'UV0')
    con(ph, '', flap, 'FlapPhase')
    con(am, '', flap, 'FlapAmount')
    con(flap, '', xf, 'Input')
    out(colr, '', 'MP_BaseColor')
    out(colr, 'Rough', 'MP_Roughness')
    out(xf, '', 'MP_WorldPositionOffset')
    return {'colour': colr, 'flap': flap, 'transform': xf}

def update_code():
    exprs = [rp(e) for e in t(M + 'get_expressions', {'material_or_function': {'refPath': MAT}})['returnValue']]
    done = []
    for e in exprs:
        if 'MaterialExpressionCustom' not in e:
            continue
        got = t(O + 'get_properties', {'instance': {'refPath': e}, 'properties': ['Description']})['returnValue']
        desc = (json.loads(got) if isinstance(got, str) else got).get('Description')
        n = {'BirdColour': 1, 'BirdFlap': 2}.get(desc)
        if n:
            sp(e, {'Code': node_code(n)})
            done.append(desc)
    # Setting only Code did not always mark the package dirty (13 Nov): nudge a material property so it saves.
    sp(MAT, {'OpacityMaskClipValue': 0.3334})
    sp(MAT, {'OpacityMaskClipValue': 0.3333})
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
