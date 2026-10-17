#!/usr/bin/env python3
"""Deterministic, offline, read-only project dashboard from the canonical matrix."""
import argparse
import hashlib
import json
from pathlib import Path
from tools.project_status.generate_status import parse_matrix, render


def build(matrix):
    rows = parse_matrix(matrix)
    for row in rows:
        cells = [c.strip() for c in row.pop('raw').strip('|').split('|')]
        if len(cells) != 8:
            raise ValueError('Expected eight matrix columns: ' + row['id'])
        row.update(risk=cells[5], next=cells[6], acceptance=cells[7])
    payload = dict(summary=render(rows)[1], rows=rows,
                   matrix_sha256=hashlib.sha256(matrix.read_bytes()).hexdigest())
    data = json.dumps(payload, ensure_ascii=False).replace('<', '\\u003c')
    return TEMPLATE.replace('__DATA__', data)


TEMPLATE = '''<!doctype html><html lang="it"><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>FUSO AVIONICS NEXT — controllo progetto</title>
<style>body{font:16px system-ui;background:#101b28;color:#e6edf5;margin:24px;max-width:1300px}h1{color:#66d9ef}input,select,button{font:inherit;padding:10px;margin:5px;background:#213249;color:white;border:1px solid #73869b;border-radius:6px}article{background:#192a3d;padding:16px;margin:10px 0;border-radius:8px}summary{cursor:pointer;font-weight:600}p{line-height:1.5}small{overflow-wrap:anywhere;color:#b8c8d8}.badge{color:#ffce75}label{display:inline-block}#stats{font-size:20px}details p{white-space:pre-wrap}</style>
<h1>FUSO AVIONICS NEXT</h1><p>Registro interattivo • sviluppo non certificato • nessuna approvazione al volo</p>
<p id="stats"></p><p>Indice ponderato convenzionale: DONE/LEGACY=1, VERIFY-HW=0,75, PARTIAL=0,5. Non misura sicurezza, collaudo o percentuale di codice completo.</p>
<small id="source"></small><p>Snapshot offline: aggiornare dal repository prima di una decisione. Non interroga i nodi e non modifica stati.</p>
<label>Ricerca <input id="search" placeholder="Sensori, terrain, ID…"></label><label>Stato <select id="status"><option value="">Tutti</option></select></label><label>Area <select id="area"><option value="">Tutte</option></select></label>
<button id="handoff">Scarica handoff filtrato</button><p id="count"></p><div id="rows"></div>
<script type="application/json" id="data">__DATA__</script>
<script>
const data=JSON.parse(document.getElementById('data').textContent), rows=data.rows;
const byId=id=>document.getElementById(id);
byId('stats').textContent=`${data.summary.requirements} requisiti • indice ${data.summary.weighted_progress}% • `+Object.entries(data.summary.status_counts).map(([s,n])=>`${s}: ${n}`).join(' / ');
byId('source').textContent='Fonte: EFIS_NEXT_MASTER_MATRIX.md • SHA-256 '+data.matrix_sha256;
for(const [id,values] of [['status',rows.map(r=>r.status)],['area',rows.map(r=>r.id.split('-')[0])]])for(const v of [...new Set(values)].sort()){const o=document.createElement('option');o.value=v;o.textContent=v;byId(id).append(o)}
let visible=[];
function refresh(){const q=byId('search').value.toLowerCase();visible=rows.filter(r=>(!byId('status').value||r.status===byId('status').value)&&(!byId('area').value||r.id.split('-')[0]===byId('area').value)&&JSON.stringify(r).toLowerCase().includes(q));byId('rows').replaceChildren();byId('count').textContent=visible.length+' requisiti visualizzati';for(const r of visible){const a=document.createElement('article'),d=document.createElement('details'),s=document.createElement('summary');s.textContent=`${r.id} • ${r.status} • ${r.requirement} — ${r.node}`;d.append(s);for(const [k,l] of [['evidence','Evidenza registrata (non riconfermata live)'],['risk','Rischi / dipendenze'],['next','Prossima azione'],['acceptance','Criterio di chiusura']]){const p=document.createElement('p');p.textContent=l+': '+r[k];d.append(p)}a.append(d);byId('rows').append(a)}}
for(const id of ['search','status','area'])byId(id).addEventListener('input',refresh);
byId('handoff').onclick=()=>{const text='FUSO AVIONICS NEXT — HANDOFF\\nMatrice SHA-256: '+data.matrix_sha256+'\\nSnapshot da riconfermare; nessuna autorizzazione merge/deploy implicita.\\n'+visible.map(r=>`\\n${r.id} ${r.status}: ${r.requirement}\\nEvidenza: ${r.evidence}\\nRischi: ${r.risk}\\nProssimo: ${r.next}\\nAcceptance: ${r.acceptance}\\n`).join('');const u=URL.createObjectURL(new Blob([text],{type:'text/plain;charset=utf-8'})),a=document.createElement('a');a.href=u;a.download='avionix-handoff.txt';a.click();setTimeout(()=>URL.revokeObjectURL(u),1000)};refresh();
</script></html>'''


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    target = Path('docs/status/PROJECT_DASHBOARD.html')
    content = build(Path('docs/status/EFIS_NEXT_MASTER_MATRIX.md'))
    if args.check:
        return 0 if target.exists() and target.read_text(encoding='utf-8') == content else 2
    target.write_text(content, encoding='utf-8')
    print('Generated ' + str(target))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
