# Scent trails fade with distance from the controlled ant (owner's note, 14 Nov 16:10): adds three scalars to MPC_View
# and, in M_Trail and M_TrailSmoke, multiplies whatever drives Opacity by a "TrailNearAnt" Custom node.
# Body of an unreal-mcp ProgrammaticToolset.execute_tool_script call (the main session pastes it; agents never write
# .uasset files).
# In place and additive: it never deletes an expression (delete_expression crashed the editor, 12 Nov). It adds nodes and
# reconnects Opacity only, so the old opacity chain (vertex alpha = line brightness, M_TrailSmoke's own camera fade and
# DepthFade) and Emissive (vertex colour x 1.1) stay as they are. A material that already has the node is left alone, so
# running it twice is safe. If a material's Opacity source cannot be found it changes nothing there and says so; then
# name it in OVERRIDE and run again. MODE = 'check' only reports what drives each material's Opacity.
# AAntCamera::PublishView sets the scalars every frame from FAntCameraTuning (TrailFadeNear / Far / Floor / Ease,
# Leaf.Tune cam): full opacity within Near of the ant, Floor x opacity by Far, and Near / Far beyond the whole tree when
# the camera is not following the ant (harvest and nest views, move card, cinematic shots). The MPC defaults mean no
# fade, so the editor viewport and any build without that C++ show trails as before.
import json
MODE = 'apply'  # 'apply' or 'check'
MPC = '/Game/Leafline/Materials/MPC_View.MPC_View'
MATS = ['/Game/Leafline/Materials/M_Trail.M_Trail', '/Game/Leafline/Materials/M_TrailSmoke.M_TrailSmoke']
# Only if the lookup fails: material -> (expression refPath, output name; '' for the first output), e.g.
# OVERRIDE = {'/Game/Leafline/Materials/M_Trail.M_Trail': ('/Game/Leafline/Materials/M_Trail.M_Trail:MaterialExpressionMultiply_2', '')}
OVERRIDE = {}
NEW = [('TrailFadeNear', 1000000.0), ('TrailFadeFar', 1000001.0), ('TrailFadeFloor', 1.0)]
CODE = '''// Scent trails fade with distance from the controlled ant (MPC_View, set by AAntCamera::PublishView): full within
// Near, Floor x the old opacity by Far (cm). Views away from the ant publish Near and Far beyond the whole tree.
float d = distance(WP.xyz, Ant.xyz);
return lerp(1.0, Floor, smoothstep(Near, max(Far, Near + 1.0), d));
'''
M = 'editor_toolset.toolsets.material.MaterialTools.'
O = 'editor_toolset.toolsets.object.ObjectTools.'
A = 'editor_toolset.toolsets.asset.AssetTools.'

def t(name, args):
    return execute_tool(name, json.dumps(args))

def rp(e):
    if isinstance(e, dict):
        return e.get('refPath') or e.get('RefPath') or ''
    return '' if e in (None, 'None', '') else str(e)

def sp(path, vals):
    return t(O + 'set_properties', {'instance': {'refPath': path}, 'values': json.dumps(vals)})['returnValue']

def gp(path, prop):
    got = t(O + 'get_properties', {'instance': {'refPath': path}, 'properties': [prop]})['returnValue']
    d = json.loads(got) if isinstance(got, str) else got
    return d.get(prop, d.get(prop[0].lower() + prop[1:]))

def key(d, base):
    for k in (base[0].lower() + base[1:], base):
        if k in d:
            return k
    return base

def grow(path, prop, items):
    # As in build_bark2_mcp.py: struct arrays only take when existing elements go back exactly as read, one per call.
    cur = gp(path, prop) or []
    changed = False
    for i in range(min(len(cur), len(items))):
        for kk, vv in items[i].items():
            k = key(cur[i], kk)
            if cur[i].get(k) != vv:
                cur[i][k] = vv
                changed = True
    if changed:
        sp(path, {prop: cur})
        cur = gp(path, prop) or []
    while len(cur) < len(items):
        it = items[len(cur)]
        el = dict(cur[-1]) if cur else dict(it)
        for kk, vv in it.items():
            el[key(el, kk)] = vv
        sp(path, {prop: cur + [el]})
        cur = gp(path, prop) or []

def add(mat, cls, x, y, props=None):
    e = rp(t(M + 'add_expression', {'material_or_function': {'refPath': mat}, 'expression_class': {'refPath': '/Script/Engine.MaterialExpression' + cls}, 'x': x, 'y': y})['returnValue'])
    if props:
        sp(e, props)
    return e

def con(a, ao, b, bi):
    t(M + 'connect_expressions', {'from_expression': {'refPath': a}, 'from_output_name': ao, 'to_expression': {'refPath': b}, 'to_input_name': bi})

def out(e, name, prop):
    t(M + 'connect_to_output', {'expression': {'refPath': e}, 'output_name': name, 'material_property': prop})

def expressions(mat):
    return [rp(e) for e in t(M + 'get_expressions', {'material_or_function': {'refPath': mat}})['returnValue']]

def description(e):
    try:
        return gp(e, 'Description')
    except Exception:
        return None

def mpc_params():
    # As in mpc_view_params_mcp.py: one new element per call.
    def get():
        return gp(MPC, 'ScalarParameters') or []
    cur = get()
    names = [p.get('parameterName', p.get('ParameterName')) for p in cur]
    added = []
    for (n, v) in NEW:
        if n in names:
            continue
        cur = cur + [{'ParameterName': n, 'DefaultValue': v}]
        sp(MPC, {'ScalarParameters': cur})
        cur = get()
        added.append(n)
    if added:
        t(A + 'save_assets', {'asset_paths': []})
    return added

def output_name(expr, idx, log):
    if not idx:
        return ''
    if 'MaterialExpressionCustom' in expr:
        outs = gp(expr, 'AdditionalOutputs') or []
        if 0 < idx <= len(outs):
            o = outs[idx - 1]
            return o.get(key(o, 'OutputName'))
    log.append('%s feeds Opacity from output %d, whose name is unknown: set OVERRIDE' % (expr, idx))
    return None

def opacity_source(mat, log):
    if mat in OVERRIDE:
        return OVERRIDE[mat]
    name = mat.rsplit('.', 1)[-1]
    # UE 5 keeps a material's output inputs on its <Name>EditorOnlyData subobject (seen in the .uasset); older: the material.
    for path in (mat + ':' + name + 'EditorOnlyData', mat):
        try:
            v = gp(path, 'Opacity')
        except Exception as ex:
            log.append('%s.Opacity: %s' % (path, str(ex)[:200]))
            continue
        if not isinstance(v, dict):
            log.append('%s.Opacity: %r' % (path, v))
            continue
        e = rp(v.get(key(v, 'Expression')))
        if not e:
            log.append('%s.Opacity is not connected: %s' % (path, json.dumps(v)[:300]))
            continue
        return e, output_name(e, v.get(key(v, 'OutputIndex'), 0) or 0, log)
    return None

def fade(mat, log):
    for e in expressions(mat):
        if 'MaterialExpressionCustom' in e and description(e) == 'TrailNearAnt':
            return 'already has TrailNearAnt: nothing changed'
    src = opacity_source(mat, log)
    if not src or src[1] is None:
        return 'Opacity source not found: nothing changed (see log)'
    if MODE != 'apply':
        return {'opacity_from': src}
    X, Y = -1100, 900
    wp = add(mat, 'WorldPosition', X, Y)
    ant = add(mat, 'CollectionParameter', X, Y + 120, {'Collection': MPC, 'ParameterName': 'AntFocus'})
    nr = add(mat, 'CollectionParameter', X, Y + 240, {'Collection': MPC, 'ParameterName': 'TrailFadeNear'})
    fr = add(mat, 'CollectionParameter', X, Y + 360, {'Collection': MPC, 'ParameterName': 'TrailFadeFar'})
    fl = add(mat, 'CollectionParameter', X, Y + 480, {'Collection': MPC, 'ParameterName': 'TrailFadeFloor'})
    node = add(mat, 'Custom', X + 380, Y + 200, {'Code': CODE, 'OutputType': 'CMOT_Float1', 'Description': 'TrailNearAnt'})
    grow(node, 'Inputs', [{'InputName': i} for i in ('WP', 'Ant', 'Near', 'Far', 'Floor')])
    for (e, n) in ((wp, 'WP'), (ant, 'Ant'), (nr, 'Near'), (fr, 'Far'), (fl, 'Floor')):
        con(e, '', node, n)
    mul = add(mat, 'Multiply', X + 700, Y + 100)
    con(src[0], src[1], mul, 'A')
    con(node, '', mul, 'B')
    out(mul, '', 'MP_Opacity')
    try:
        t(M + 'recompile', {'material_or_function': {'refPath': mat}})
    except Exception as ex:
        log.append('%s recompile: %s' % (mat, str(ex)[:1500]))
    after = opacity_source(mat, log)
    return {'old_opacity_from': src, 'opacity_now_from': after, 'nodes': [wp, ant, nr, fr, fl, node, mul]}

def run():
    log = []
    res = {'mode': MODE}
    if MODE == 'apply':
        res['mpc_added'] = mpc_params()
    for mat in MATS:
        res[mat] = fade(mat, log)
    if MODE == 'apply':
        t(A + 'save_assets', {'asset_paths': []})
    res['log'] = log
    return res
