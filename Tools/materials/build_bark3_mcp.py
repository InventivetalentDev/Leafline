# Builds /Game/Leafline/Materials/M_TreeBark3 as a DUPLICATE of M_TreeBark2 (its cutaway, buried test, shadow pass and
# outputs stay exactly as they are) and replaces only the Code of the Custom node "Bark" with bark3_bark.hlsl, adding two
# inputs: UV0 (TextureCoordinate 0) and VA (the vertex colour's A output). The C++ side (leaf.SkirtBlend, set by
# Leaf.Bark 2) writes the seam cross-fade data into those channels; see the header of bark3_bark.hlsl.
# It is the body of an unreal-mcp ProgrammaticToolset.execute_tool_script call (the main session pastes it; agents never
# write .uasset files). dup() guesses the duplicate tool's argument names as build_ant2_mcp.py does; if none fits,
# duplicate M_TreeBark2 to M_TreeBark3 by hand in the content browser and set DUP = False.
# MODE='code' only replaces the Bark node's Code on an existing M_TreeBark3 (after editing the HLSL): never delete and
# recreate an asset of the same name in one editor session (CLAUDE.md).
import json
MODE = 'build'  # 'build' duplicates M_TreeBark2 and rewires the Bark node; 'code' updates its Code and recompiles
DUP = True
HLSL = 'D:/UnrealProjects/AntTreeGame/Tools/materials/bark3_bark.hlsl'  # after the merge; else the seams worktree's copy
SRC = '/Game/Leafline/Materials/M_TreeBark2.M_TreeBark2'
MAT = '/Game/Leafline/Materials/M_TreeBark3.M_TreeBark3'
M = 'editor_toolset.toolsets.material.MaterialTools.'
O = 'editor_toolset.toolsets.object.ObjectTools.'
A = 'editor_toolset.toolsets.asset.AssetTools.'

def t(name, args):
    return execute_tool(name, json.dumps(args))

def rp(e):
    return e['refPath'] if isinstance(e, dict) else str(e)

def code():
    with open(HLSL, 'r') as f:
        return f.read()

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
    # As build_bark2_mcp.py: struct arrays only take when the existing elements go back exactly as read and one element
    # is appended per call. Existing inputs keep their names and links; new ones are linked below.
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

def con(a, ao, b, bi):
    t(M + 'connect_expressions', {'from_expression': {'refPath': a}, 'from_output_name': ao, 'to_expression': {'refPath': b}, 'to_input_name': bi})

def dup():
    tries = [
        {'source_asset_path': SRC, 'destination_path': '/Game/Leafline/Materials/M_TreeBark3'},
        {'source_path': SRC, 'destination_path': '/Game/Leafline/Materials/M_TreeBark3'},
        {'asset_path': SRC, 'new_path': '/Game/Leafline/Materials/M_TreeBark3'},
        {'source': SRC, 'destination': '/Game/Leafline/Materials/M_TreeBark3'},
    ]
    last = None
    for a in tries:
        try:
            return t(A + 'duplicate_asset', a)
        except Exception as ex:
            last = str(ex)[:600]
    raise Exception('duplicate_asset failed: ' + str(last))

def bark_node():
    exprs = [rp(e) for e in t(M + 'get_expressions', {'material_or_function': {'refPath': MAT}})['returnValue']]
    for e in exprs:
        if 'MaterialExpressionCustom' not in e:
            continue
        got = t(O + 'get_properties', {'instance': {'refPath': e}, 'properties': ['Description']})['returnValue']
        if (json.loads(got) if isinstance(got, str) else got).get('Description') == 'Bark':
            return e
    raise Exception('no Custom node "Bark" in ' + MAT)

def build():
    log = []
    if DUP:
        dup()
    bark = bark_node()
    names = [i.get('InputName', i.get('inputName')) for i in gp(bark, 'Inputs')]
    log.append('Bark inputs before: %s' % names)
    sp(bark, {'Code': code()})
    want = [{'InputName': n} for n in names]
    for n in ('UV0', 'VA'):
        if n not in names:
            want.append({'InputName': n})
    grow(bark, 'Inputs', want)
    uv0 = add('TextureCoordinate', -1800, -760, {'CoordinateIndex': 0})
    vc = add('VertexColor', -1800, -880)
    con(uv0, '', bark, 'UV0')
    con(vc, 'A', bark, 'VA')
    log.append('Bark inputs after: %s' % [i.get('InputName', i.get('inputName')) for i in gp(bark, 'Inputs')])
    return {'bark': bark, 'log': log}

def update_code():
    bark = bark_node()
    sp(bark, {'Code': code()})
    # Setting only Code did not always mark the package dirty (13 Nov): nudge a material property so it saves.
    sp(MAT, {'OpacityMaskClipValue': 0.5001})
    sp(MAT, {'OpacityMaskClipValue': 0.5})
    return {'updated': bark}

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
