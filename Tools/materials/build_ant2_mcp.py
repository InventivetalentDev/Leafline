# Builds /Game/Leafline/Materials/M_Ant2 as a DUPLICATE of M_Ant (its gait WPO chain stays as it is) and replaces only the
# colour side with the Custom node in Tools/materials/ant_look.hlsl. dup() guesses the duplicate tool's argument names;
# if none fits, duplicate M_Ant to M_Ant2 by hand and comment the dup() call out.
# It is the body of an unreal-mcp ProgrammaticToolset.execute_tool_script call (the main session pastes it; agents never
# write .uasset files). MODE='code' only replaces the AntLook node's Code on an existing M_Ant2: never delete and recreate an
# asset of the same name in one editor session (CLAUDE.md).
import json
MODE = 'build'  # 'build' duplicates M_Ant and rewires the colour side; 'code' updates the Code of the AntLook node and recompiles
HLSL = 'D:/UnrealProjects/leafline-floor/Tools/materials/ant_look.hlsl'
SRC = '/Game/Leafline/Materials/M_Ant.M_Ant'
MAT = '/Game/Leafline/Materials/M_Ant2.M_Ant2'
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


def dup():
    # First call to swap if the tool differs: AssetTools.duplicate_asset(source, destination folder/name).
    # Try the likely argument spellings in turn.
    tries = [
        {'source_asset_path': SRC, 'destination_path': '/Game/Leafline/Materials/M_Ant2'},
        {'source_path': SRC, 'destination_path': '/Game/Leafline/Materials/M_Ant2'},
        {'asset_path': SRC, 'new_path': '/Game/Leafline/Materials/M_Ant2'},
        {'source': SRC, 'destination': '/Game/Leafline/Materials/M_Ant2'},
    ]
    last = None
    for a in tries:
        try:
            return t(A + 'duplicate_asset', a)
        except Exception as ex:
            last = str(ex)[:600]
    raise Exception('duplicate_asset failed: ' + str(last))

def find_expr(exprs, cls, param=None):
    out_ = []
    for e in exprs:
        if cls in e:
            if param is None:
                out_.append(e)
            else:
                got = t(O + 'get_properties', {'instance': {'refPath': e}, 'properties': ['ParameterName']})['returnValue']
                if (json.loads(got) if isinstance(got, str) else got).get('ParameterName') == param:
                    out_.append(e)
    return out_

def build():
    log = []
    dup()
    exprs = [rp(e) for e in t(M + 'get_expressions', {'material_or_function': {'refPath': MAT}})['returnValue']]
    log.append('expressions in copy: %d' % len(exprs))
    X = -1500
    uv0 = add('TextureCoordinate', X, -300, {'CoordinateIndex': 0})
    uv1 = add('TextureCoordinate', X, -180, {'CoordinateIndex': 1})
    # Local Position: LocalOrigin Primitive, IncludedOffsets ExcludeOffsets (PreSkinnedLocalPosition is not accepted).
    lp = add('LocalPosition', X, 0, {'LocalOrigin': 'LPO_Primitive', 'IncludedOffsets': 'LPIO_ExcludeOffsets'})
    fr = add('Fresnel', X, 120, {'Exponent': 5.0})
    htc = add('VectorParameter', X, 260, {'ParameterName': 'HeadThoraxColor', 'DefaultValue': {'r': 0.60, 'g': 0.22, 'b': 0.08, 'a': 1.0}})
    gc = add('VectorParameter', X, 380, {'ParameterName': 'GasterColor', 'DefaultValue': {'r': 0.34, 'g': 0.11, 'b': 0.05, 'a': 1.0}})
    lc = add('VectorParameter', X, 500, {'ParameterName': 'LegColor', 'DefaultValue': {'r': 0.50, 'g': 0.20, 'b': 0.09, 'a': 1.0}})
    ro = add('ScalarParameter', X, 620, {'ParameterName': 'Roughness', 'DefaultValue': 0.38})
    fi = add('ScalarParameter', X, 700, {'ParameterName': 'Fill', 'DefaultValue': 0.05})
    ri = add('ScalarParameter', X, 780, {'ParameterName': 'RimStrength', 'DefaultValue': 0.0})
    node = custom(-900, 0, 'AntLook', 1, ['UV0', 'UV1', 'LP', 'Fres', 'HeadThoraxColor', 'GasterColor', 'LegColor', 'Roughness', 'Fill', 'RimStrength'],
                  'CMOT_Float3', [('Rough', 'CMOT_Float1'), ('Emit', 'CMOT_Float3')])
    for src_e, nm in ((uv0, 'UV0'), (uv1, 'UV1'), (lp, 'LP'), (fr, 'Fres'), (htc, 'HeadThoraxColor'), (gc, 'GasterColor'),
                      (lc, 'LegColor'), (ro, 'Roughness'), (fi, 'Fill'), (ri, 'RimStrength')):
        con(src_e, '', node, nm)
    # Replaces the old BaseColor (Lerp of BodyColor) and Emissive (the Multiply_11 rim) connections; connecting a new
    # expression to a material output replaces the old link. The old nodes stay, unused; the WPO chain is not touched.
    out(node, '', 'MP_BaseColor')
    out(node, 'Rough', 'MP_Roughness')
    out(node, 'Emit', 'MP_EmissiveColor')
    return {'node': node, 'log': log}

def update_code():
    exprs = [rp(e) for e in t(M + 'get_expressions', {'material_or_function': {'refPath': MAT}})['returnValue']]
    done = []
    for e in exprs:
        if 'MaterialExpressionCustom' not in e:
            continue
        got = t(O + 'get_properties', {'instance': {'refPath': e}, 'properties': ['Description']})['returnValue']
        desc = (json.loads(got) if isinstance(got, str) else got).get('Description')
        n = {'AntLook': 1}.get(desc)
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
