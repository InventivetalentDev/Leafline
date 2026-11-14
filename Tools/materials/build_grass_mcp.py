# MODE 'build2' (v2, 14 Nov evening): builds /Game/Leafline/Materials/M_Grass2 (a new asset: never recreate a name in one editor
# session); MODE 'code1': only replaces the Code of the existing M_Grass node with NODE 1 of grass.hlsl.
# Builds /Game/Leafline/Materials/M_Grass for the forest floor's grass blades (AForestFloor): matte, opaque, default lit,
# one-sided (the floor emits both windings), Roughness 1, Specular 0, Base Colour from a vector parameter 'Color' (the
# floor sets it on the dynamic instance), and a World Position Offset that sways each blade downwind: one Custom node from
# Tools/materials/grass.hlsl fed by TexCoord 0 and 1, MPC_Wind (WindDirection, WindStrength), Time and Color.
# It is the body of an unreal-mcp ProgrammaticToolset.execute_tool_script call (the main session pastes it; agents never
# write .uasset files). Run once: never delete and recreate an asset of the same name in one editor session (CLAUDE.md).
import json
HLSL = 'D:/UnrealProjects/AntTreeGame/Tools/materials/grass.hlsl'
MPC = '/Game/Leafline/Materials/MPC_Wind.MPC_Wind'
MODE = 'build2'   # 'build2' creates M_Grass2 (NODE 2: backlit foliage + fill, dark foot); 'code1' replaces the Code of M_Grass's node with NODE 1 (dark foot only)
NAME = 'M_Grass2' if MODE == 'build2' else 'M_Grass'
MAT = '/Game/Leafline/Materials/%s.%s' % (NAME, NAME)
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



def mpc(name, x, y):
    return add('CollectionParameter', x, y, {'Collection': MPC, 'ParameterName': name})

def set_color(e, r, g, b):
    # The struct property's spelling differs between tool versions: try them in turn, ignore a default that will not take.
    for v in ({'r': r, 'g': g, 'b': b, 'a': 1.0}, {'R': r, 'G': g, 'B': b, 'A': 1.0}, '(R=%s,G=%s,B=%s,A=1.0)' % (r, g, b)):
        try:
            sp(e, {'DefaultValue': v})
            return True
        except Exception:
            pass
    return False

def con_any(a, outs, b, bi):
    # A vector parameter's colour output is named differently between tool versions ('RGB' or ''): try each.
    last = None
    for o in outs:
        try:
            con(a, o, b, bi)
            return o
        except Exception as ex:
            last = str(ex)[:300]
    raise Exception('connect failed: ' + str(last))

def try_set(path, candidates):
    # Some property spellings differ between tool versions: try each, report which took.
    for vals in candidates:
        try:
            sp(path, vals)
            return vals
        except Exception:
            pass
    return None

def run():
    if MODE == 'code1':
        exprs = [rp(e) for e in t(M + 'get_expressions', {'material_or_function': {'refPath': MAT}})['returnValue']]
        done = []
        for e in exprs:
            if 'MaterialExpressionCustom' not in e:
                continue
            got = t(O + 'get_properties', {'instance': {'refPath': e}, 'properties': ['Description']})['returnValue']
            if (json.loads(got) if isinstance(got, str) else got).get('Description') == 'GrassSway':
                sp(e, {'Code': node_code(1)})
                done.append(e)
        sp(MAT, {'OpacityMaskClipValue': 0.3334})   # the save nudge (setting only Code did not always mark the package dirty)
        sp(MAT, {'OpacityMaskClipValue': 0.3333})
        t(M + 'recompile', {'material_or_function': {'refPath': MAT}})
        t(A + 'save_assets', {'asset_paths': []})
        return {'updated': done}
    t(M + 'create_material', {'folder_path': '/Game/Leafline/Materials', 'asset_name': NAME})
    X = -1100
    uv0 = add('TextureCoordinate', X, -300, {'CoordinateIndex': 0})
    uv1 = add('TextureCoordinate', X, -180, {'CoordinateIndex': 1})
    wd = mpc('WindDirection', X, 0)
    ws = mpc('WindStrength', X, 120)
    tm = add('Time', X, 240)
    col = add('VectorParameter', X, 360, {'ParameterName': 'Color'})
    ok = set_color(col, 0.028, 0.070, 0.032)
    node = custom(-650, 0, 'GrassSway', 2, ['UV0', 'UV1', 'Wind', 'Strength', 'Time', 'Color'], 'CMOT_Float3',
                  [('Col', 'CMOT_Float3'), ('Sub', 'CMOT_Float3'), ('Emit', 'CMOT_Float3')])
    con(uv0, '', node, 'UV0')
    con(uv1, '', node, 'UV1')
    con(wd, '', node, 'Wind')
    con(ws, '', node, 'Strength')
    con(tm, '', node, 'Time')
    used = con_any(col, ['RGB', ''], node, 'Color')
    rough = add('Constant', -650, 400, {'R': 1.0})
    spec = add('Constant', -650, 500, {'R': 0.0})
    out(node, 'Col', 'MP_BaseColor')
    out(node, 'Sub', 'MP_SubsurfaceColor')
    out(node, 'Emit', 'MP_EmissiveColor')
    out(node, '', 'MP_WorldPositionOffset')
    out(rough, '', 'MP_Roughness')
    out(spec, '', 'MP_Specular')
    # Two Sided Foliage: light from behind comes through the blade (Subsurface Color). The property's spelling differs
    # between engine versions; if none takes the material stays default lit and the emissive floor alone keeps blades off black.
    sm = try_set(MAT, [{'ShadingModel': 'MSM_TwoSidedFoliage'}, {'ShadingModel': 'TwoSidedFoliage'},
                       {'ShadingModels': {'bIsTwoSidedFoliage': True}}, {'ShadingModels': '(ShadingModelField=65536)'}])
    errs = []
    try:
        t(M + 'recompile', {'material_or_function': {'refPath': MAT}})
    except Exception as ex:
        errs.append(str(ex)[:1500])
    t(A + 'save_assets', {'asset_paths': []})
    return {'color_default_set': ok, 'color_output_used': used, 'shading_model_set': sm, 'node': node, 'errors': errs}
