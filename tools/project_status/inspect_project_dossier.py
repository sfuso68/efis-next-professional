"""Static PDF content/bounds checks and contact sheets for human visual review."""
import argparse
import json
from pathlib import Path
import re
from PIL import Image, ImageDraw
import pdfplumber
from tools.project_status.project_dossier import collect


def inspect(pdf,rendered,output):
    snapshot=collect();texts=[];bounds=[]
    with pdfplumber.open(pdf) as doc:
        for n,page in enumerate(doc.pages,1):
            text=page.extract_text() or "";texts.append(text)
            if len(text)<90:raise ValueError(f"Almost empty page {n}")
            for ch in page.chars:
                if ch["x0"]<43 or ch["x1"]>page.width-43 or ch["top"]<25 or ch["bottom"]>page.height-20:
                    bounds.append(dict(page=n,text=ch["text"],x=ch["x0"],top=ch["top"]))
        pages=len(doc.pages)
    joined="\n".join(texts)
    missing=[r["id"] for r in snapshot["requirements"] if not re.search(re.escape(r["id"])+r"\b",joined)]
    if missing:raise ValueError(f"Missing requirements: {missing}")
    if "\ufffd" in joined or "\u25a0" in joined:raise ValueError("Missing glyph marker")
    if bounds:raise ValueError(f"Out of page content bounds: {bounds[:8]}")
    images=sorted(Path(rendered).glob("page-*.png"))
    if len(images)!=pages:raise ValueError("Render/PDF page count mismatch")
    output=Path(output);output.mkdir(parents=True,exist_ok=True)
    for start in range(0,pages,6):
        sheet=Image.new("RGB",(1090,1080),"#DCE5EB");draw=ImageDraw.Draw(sheet)
        for pos,path in enumerate(images[start:start+6]):
            with Image.open(path) as image:
                image.thumbnail((340,490));x=15+(pos%3)*360;y=25+(pos//3)*530
                sheet.paste(image,(x,y));draw.text((x,y+495),f"Page {start+pos+1}",fill="black")
        sheet.save(output/f"contact-{start//6+1}.png")
    report=dict(status="STATIC_CHECK_PASS_REQUIRES_VISUAL_REVIEW",pages=pages,
                requirements=len(snapshot["requirements"]),missing=[],out_of_bounds=[])
    (output/"pdf-static-check.json").write_text(json.dumps(report,indent=2),encoding="utf-8")
    print(json.dumps(report))


if __name__=="__main__":
    p=argparse.ArgumentParser();p.add_argument("pdf",type=Path);p.add_argument("rendered",type=Path)
    p.add_argument("output",type=Path);a=p.parse_args();inspect(a.pdf,a.rendered,a.output)
