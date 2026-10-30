"""Evidence-bound EFIS project dossier. Offline reporting, no avionics mutation."""
import argparse
from datetime import date
import hashlib
import json
from pathlib import Path
import subprocess
from xml.sax.saxutils import escape

from tools.project_status.generate_status import parse_matrix, render

ROOT = Path(__file__).resolve().parents[2]
SOURCES = ["docs/status/EFIS_NEXT_STATE.md", "docs/status/EFIS_NEXT_MASTER_MATRIX.md",
    "docs/status/EFIS_NEXT_ACTIVE_RELEASES.md", "docs/status/SIMULATOR_COMPLETION_CHECKLIST.md",
    "config/simulation/development-plan-v1.json",
    "docs/status/evidence/r1-28-sil-run-restore-20261028/verification.json",
    "docs/adr/ADR-003-ingestion-and-transport.md", "docs/adr/ADR-010-avionics-ethernet-segmentation.md",
    "docs/adr/ADR-012-emergency-assist-command-boundary.md",
    "docs/adr/ADR-013-compute-platform-allocation.md", "docs/adr/ADR-014-voice-radio-assist-boundary.md",
    "docs/adr/ADR-018-git-repository-topology.md"]


def collect(root=ROOT):
    rows = parse_matrix(root/SOURCES[1])
    _, status = render(rows)
    generated = json.loads((root/"docs/status/efis-next-status.json").read_text(encoding="utf-8"))
    if status != generated: raise ValueError("Generated status is stale; regenerate before publishing")
    plan = json.loads((root/SOURCES[4]).read_text(encoding="utf-8"))
    evidence = json.loads((root/SOURCES[5]).read_text(encoding="utf-8"))
    if plan["completion"] != "NOT_COMPLETE" or plan["flight_approval"] is not False:
        raise ValueError("Dossier cannot infer simulator or flight acceptance")
    for row in rows:
        cells = [x.strip().strip("`") for x in row.pop("raw").strip("|").split("|")]
        if len(cells) != 8: raise ValueError("Requirement matrix column mismatch")
        row.update(risk=cells[5], next_step=cells[6], acceptance=cells[7])
    head = subprocess.check_output(["git","-c",f"safe.directory={root}","rev-parse","HEAD"],cwd=root,text=True).strip()
    branch = subprocess.check_output(["git","-c",f"safe.directory={root}","branch","--show-current"],cwd=root,text=True).strip()
    return dict(title="FUSO AVIONICS NEXT",date="2026-10-28",head=head,branch=branch,
        repository="sfuso68/efis-next-professional",scope="DOCUMENTARY_SNAPSHOT_NOT_LIVE_NODE_AUDIT",
        flight_approval=False,status=status,requirements=rows,simulation=plan,evidence=evidence,
        sources=[dict(path=p,sha256=hashlib.sha256((root/p).read_bytes()).hexdigest()) for p in SOURCES])


def build(output, snapshot):
    from reportlab.lib import colors
    from reportlab.lib.enums import TA_LEFT
    from reportlab.lib.pagesizes import A4
    from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
    from reportlab.pdfbase import pdfmetrics
    from reportlab.pdfbase.ttfonts import TTFont
    from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle, PageBreak, Flowable, KeepTogether

    fonts=Path("C:/Windows/Fonts")
    if (fonts/"segoeui.ttf").exists():
        pdfmetrics.registerFont(TTFont("Dossier",str(fonts/"segoeui.ttf")))
        pdfmetrics.registerFont(TTFont("DossierBold",str(fonts/"segoeuib.ttf")))
    else:
        import reportlab
        base=Path(reportlab.__file__).parent/"fonts"
        pdfmetrics.registerFont(TTFont("Dossier",str(base/"Vera.ttf")))
        pdfmetrics.registerFont(TTFont("DossierBold",str(base/"VeraBd.ttf")))
    pdfmetrics.registerFontFamily("Dossier",normal="Dossier",bold="DossierBold",italic="Dossier",boldItalic="DossierBold")
    navy=colors.HexColor("#102B40"); cyan=colors.HexColor("#007E98")
    light=colors.HexColor("#EDF4F7"); gray=colors.HexColor("#516170")
    styles=getSampleStyleSheet()
    styles.add(ParagraphStyle(name="BodyD",fontName="Dossier",fontSize=9.4,leading=14,spaceAfter=8,textColor=navy))
    styles.add(ParagraphStyle(name="SmallD",parent=styles["BodyD"],fontSize=7.7,leading=11,spaceAfter=4))
    styles.add(ParagraphStyle(name="H1D",fontName="DossierBold",fontSize=21,leading=26,spaceAfter=15,textColor=navy))
    styles.add(ParagraphStyle(name="H2D",fontName="DossierBold",fontSize=12.2,leading=17,spaceBefore=9,spaceAfter=6,textColor=cyan,keepWithNext=True))
    styles.add(ParagraphStyle(name="LabelD",fontName="DossierBold",fontSize=8,leading=11,textColor=cyan,spaceAfter=7))
    story=[]; sections=[]
    def p(text,style="BodyD"):
        text=str(text).replace("\u2011","-").replace("\u2013","-").replace("\u2014","-")
        return Paragraph(escape(text).replace("\n","<br/>"),styles[style])
    def text(value):story.append(p(value))
    def sub(title,value):story.extend([p(title,"H2D"),p(value)])
    def section(title):
        if story:story.append(PageBreak())
        sections.append(title);story.extend([p("PROGETTO / STATO E SVILUPPO","LabelD"),p(title,"H1D")])
    def table(headers,rows,widths):
        data=[[p(h,"SmallD") for h in headers]]+[[p(v,"SmallD") for v in row] for row in rows]
        t=Table(data,colWidths=widths,repeatRows=1,hAlign="LEFT")
        t.setStyle(TableStyle([("BACKGROUND",(0,0),(-1,0),light),
            ("VALIGN",(0,0),(-1,-1),"TOP"),("LINEBELOW",(0,0),(-1,0),1,cyan),
            ("LINEBELOW",(0,1),(-1,-1),.3,colors.HexColor("#D9E2E8")),
            ("LEFTPADDING",(0,0),(-1,-1),7),("RIGHTPADDING",(0,0),(-1,-1),7),
            ("TOPPADDING",(0,0),(-1,-1),7),("BOTTOMPADDING",(0,0),(-1,-1),7)]))
        story.extend([t,Spacer(1,9)])

    class Map(Flowable):
        def __init__(self,kind):Flowable.__init__(self);self.width=499;self.height=420;self.kind=kind
        def draw(self):
            cv=self.canv
            def box(x,y,w,h,title,lines):
                cv.setFillColor(light);cv.setStrokeColor(cyan);cv.roundRect(x,y,w,h,7,fill=1)
                cv.setFillColor(navy);cv.setFont("DossierBold",10);cv.drawString(x+10,y+h-19,title)
                cv.setFont("Dossier",8)
                for i,line in enumerate(lines):cv.drawString(x+10,y+h-36-i*12,line)
            def arrow(x1,y1,x2,y2):
                cv.setStrokeColor(cyan);cv.setLineWidth(1.4);cv.line(x1,y1,x2,y2)
                import math
                angle=math.atan2(y2-y1,x2-x1)
                for da in (-.45,.45):cv.line(x2,y2,x2-7*math.cos(angle+da),y2-7*math.sin(angle+da))
            if self.kind=="system":
                box(0,325,149,80,"DATI DI VOLO",["Pixhawk 6X / M10","Matek DLVR / DroneCAN","Solo acquisizione EFIS"])
                box(350,325,149,80,"SENSORI MOTORE",["RPi4 sensor / Sequent","Sonde, calibrazione","Catena reale incompleta"])
                box(170,210,159,85,"BLACKBOX / RPi5",["Arbitraggio e validita'","Broker, log, edge weather","Nessun comando attuatore"])
                arrow(75,325,205,295);arrow(425,325,294,295)
                for x,title,lines in [(0,"PFD / RPi5",["Assetto e dati aria","SVS / emergenze"]),(175,"MFD / RPi5",["NAV / Canvas / meteo","Piano di volo"]),(350,"EMS / RPi4",["Strumenti motore","Impianti e allarmi"])]:
                    box(x,90,149,80,title,lines);arrow(249,210,x+74,170)
                box(0,0,235,60,"VIDEO SEPARATO",["4 camere -> RPi4 FPV dedicato","Mai condiviso con RPi4 EMS"])
                box(264,0,235,60,"SERVER / NAS / SIL",["Dati, archivi, sviluppo e test","Non indispensabili al volo"])
            else:
                box(0,325,499,75,"INGRESSI E FONTI",["Adapter read-only: MAVLink / CAN / sensori / replay","Per ogni campione: origine, tempo, sequenza, unita', qualita', validita'"])
                box(80,215,339,80,"CORE C++20 SENZA QT",["Normalizzazione -> arbitraggio -> freshness -> snapshot","Health, allarmi consultivi, profili, audit e degradazione"])
                arrow(249,325,249,295)
                box(0,90,235,85,"PRESENTAZIONE",["Qt Quick/QML e componenti condivisi","PFD / MFD / Canvas / EMS / setup","Scale, dati mancanti e SIM espliciti"])
                box(264,90,235,85,"DATI E REGISTRAZIONE",["DDP flight / EDP engine","NAV-WX-airframe: contratti distinti","Blackbox, cache, replay e restore"])
                arrow(180,215,117,175);arrow(320,215,381,175)
                box(0,0,499,60,"PROFILI STANDARD E PROFESSIONAL",["Una codebase, capability e configurazioni; non due copie divergenti.","Complessita' maggiore non equivale a qualifica o ridondanza accettata."])

    # 1: cover, deliberately not a claim that the implementation is complete.
    story.extend([Spacer(1,65),p("FUSO AVIONICS NEXT","H1D"),
        p("Progetto completo e stato di avanzamento","H1D"),
        p("STANDARD + PROFESSIONAL","H2D"),Spacer(1,18)])
    text("Architettura, peculiarita', mappa del sistema, funzioni, evidenze, rischi e piano di completamento.")
    text("Edizione documentale: 28 ottobre 2026. Documento completo nel perimetro; software e integrazione ancora parziali.")
    table(["Identita'","Baseline consultata"],[("Repository",snapshot["repository"]),("Branch",snapshot["branch"]),
        ("Commit",snapshot["head"]),("Stato prodotto","DEVELOPMENT / NON-CERT / NON APPROVATO AL VOLO")],[105,394])
    text("Snapshot documentale e software. Nessun nuovo audit live dei nodi o rollout e' implicito in questa edizione. Le date delle osservazioni reali prevalgono su descrizioni generiche o storiche.")
    sub("Come leggere il dossier","Prima: sintesi, mappe e differenze dei profili. Poi: sottosistemi, simulatore, gate e planning. L'appendice conserva tutti i 71 requisiti con prossimo passo e criterio di chiusura; la matrice versionata resta la fonte operativa.")

    section("01 / Sintesi immediata")
    status=snapshot["status"]
    text("EFIS NEXT e' una piattaforma avionica distribuita per sviluppo sperimentale VDS/LSA e aviazione generale. Unisce strumenti di volo, motore, navigazione, synthetic vision, meteo, registrazione e funzioni consultive, mantenendo distinti sensori, core e interfaccia.")
    table(["Stato","Requisiti","Significato"],[(key,str(status["status_counts"].get(key,0)),meaning) for key,meaning in [
        ("DONE","Chiusura documentata del criterio specifico, non qualifica dell'intero prodotto."),
        ("VERIFY-HW","Prova hardware o accettazione ancora necessaria."),("PARTIAL","Base presente ma criterio completo non chiuso."),
        ("TODO","Lavoro ancora da realizzare."),("BLOCKED","Dipendenza/autorita' non risolta."),("LEGACY","Progetto storico separato, non nuova funzione.")]],[82,62,355])
    text(f"Indice ponderato della matrice: {status['weighted_progress']}% su {status['requirements']} requisiti. E' una metrica gestionale, NON una percentuale di sicurezza, affidabilita' o prontezza al volo. LEGACY pesa nel calcolo storico; non rappresenta nuovo sviluppo.")
    sub("Cosa distingue il progetto","Unica codebase; provenance e validita' prima della grafica; degradazione esplicita; due profili; moduli distribuiti; terreno e dati offline; simulazione per le stesse applicazioni EFIS, non un visualizzatore esterno alternativo.")
    sub("Cosa impedisce oggi una dichiarazione di completamento","Integrazione sensori e limiti reali, console SIL e multi-panel completi, modelli specifici verificati, alimentazione e acceptance grafica, video/audio reali, catalogo/restore NAS e prove ambientali/endurance.")

    section("02 / Mappa fisica del sistema")
    story.extend([Map("system"),Spacer(1,12)])
    text("Le frecce indicano responsabilita' funzionali, non attestano che ogni collegamento sia gia' operativo. La Blackbox normalizza e distribuisce; PFD/MFD non interpretano MAVLink grezzo. Il sensore motore e il VIDEO-CORE sono nodi distinti.")
    sub("Confine essenziale","Il server di casa e il NAS supportano aggiornamenti, dati e test, ma non devono essere necessari per continuare a presentare informazioni locali in volo. La Blackbox e' un punto comune di guasto da gestire con STALE/LOST, non una ridondanza gia' qualificata.")

    section("03 / Mappa software e filosofia")
    story.extend([Map("software"),Spacer(1,12)])
    text("C++20 contiene il dominio; Qt/QML e' l'adapter grafico. Il limite attuale non dimostra un'incapacita' di Qt: layout, font, scale, shader, prestazioni e accettazione sul display richiedono lavoro specifico. Non e' pianificato un cambio indiscriminato di framework.")
    sub("Dati prima dell'aspetto","Un numero plausibile non e' sufficiente. Origine, eta', unita', calibrazione e stato SIM devono accompagnare il dato. Un canale mancante deve restare indisponibile; non diventare zero, verde o SYSTEM NORMAL.")

    section("04 / Standard e Professional")
    text("Due profili di prodotto della stessa piattaforma, non due repository e non due implementazioni divergenti. La tabella indica l'assetto di progetto/capability, NON due pacchetti commerciali gia' integralmente verificati. PROD-001 e' ancora TODO nella matrice: la selezione completa e validata delle capability per i due profili resta da chiudere.")
    table(["Area","Standard","Professional"],[
        ("Fondazione","Core, validita', PFD e setup condivisi","Stessa fondazione; piu' capability e nodi"),
        ("Display","PFD e strumenti essenziali, banda EMS","PFD + MFD/Canvas + EMS dedicato"),
        ("Navigazione","GPS e funzioni base previste","Piano volo, radio NAV, diversioni e dati avanzati"),
        ("Synthetic vision","Semplificata/opzionale secondo profilo","Terreno, ostacoli e TAWS come target avanzato"),
        ("Motore","EMS e logging secondo configurazione","Impianti estesi, calibrazione, diagnostica e manutenzione"),
        ("Meteo/dati","Cache e funzioni selezionate","Distribuzione edge e cataloghi/pacchetti evoluti"),
        ("FPV/accessori","Capability opzionali, non obbligo","Multi-camera e strumenti accessori nel perimetro"),
        ("Safety","Nessuna validita' fittizia o auto-attuazione","Stessi vincoli; piu' funzioni non autorizzano comandi"),
        ("Qualificazione","Da accettare sul proprio allestimento","Da accettare su tutti i nodi e failure mode")],[102,197,200])
    sub("Peculiarita' da preservare","Funzionamento offline; configurazione per singolo esemplare; setup con audit e rollback; grafica comune scalabile; separazione EMS/FPV; nessuna equivalenza automatica fra ASSO 5 e Pioneer 300 o fra Rotax 912 UL e ULS.")

    section("05 / Nodi e inventario noto")
    text("Indirizzi di manutenzione osservati sulla LAN: non prova di raggiungibilita' attuale. Release riportate come ultime osservate, non tutte rilette il 28 ottobre. Il registro canonico contiene date, puntatori ed evidenze.")
    table(["Nodo / indirizzo","Ruolo e ultimo riferimento","Da riconfermare o completare"],[
        ("PFD RPi5 / .45","efis-pfd-01; pfdrec-7051a47-retention-v1. Osservazioni SIL fino al 26/10 con renderer invariato.","Touch PFD aperto; acceptance grafica e soak; nessun nuovo rollout SIL permanente."),
        ("MFD RPi5 / .49","pi5; mfd-3432402-primary-v1. Preflight 25/10: processo/puntatore osservati.","Undervoltage recenti, power gate, nuova grafica/golden e prestazioni."),
        ("EMS RPi4 / .36","efis; ems-a0be9a5-main-v1 osservata nel preflight 25/10, SIM.","Sorgenti reali, calibrazione, allarmi/ore reali e acceptance attuale."),
        ("Blackbox RPi5 / .47","efisblackbox; 7655799-v1, broker/engine-simulation; osservazioni precedenti.","Power, NAV fail-closed, CAN/endurance; identita' e release da riconfermare prima di rollout."),
        ("Sensor RPi4 / .89","ems-sensor; nessuna release distribuita; I2C e identita' documentati.","Inventario fisico, DIP, schede Sequent, channel map e driver validato."),
        ("FPV RPi4 dedicato","Secondo RPi4 fisicamente distinto dall'EMS.","Nodo/camere/decoder/pipeline da completare e misurare."),
        ("Serverha / .6","SIL JSBSim isolato, mappe, servizi e mirror Git. Secondo SSD test autorizzato.","Inventario servizi canonico, cataloghi e qualifica storage/restore periodico."),
        ("NAS / .22","Sanfrancesco34, archivio Git e backup separato.","Restore specifico SIL da NAS non ancora provato."),
        ("Portatile / .39","QGC/observer Linux; host di manutenzione.","Versioni correnti, timing e regressione firewall/rete."),
        ("Windows / .33","FS2024 Steam e SDK su D: documentati; non requisito del SIL autonomo.","Bridge opzionale finale da qualificare; non presumere test MSFS pienamente riusciti.")],[104,201,194])

    section("06 / PFD, Canvas e synthetic vision")
    sub("Base realizzata","PFD adattivo, nastri IAS/ALT/VS, assetto e bussole, FMA, NAV, banda EMS e Canvas. Header e validita' dei canali rivisti sui candidati; meteo/airframe mancanti non devono apparire veri. Componenti QML condivisi fra applicazioni.")
    sub("Obiettivo grafico","Alta leggibilita', scale graduate, valori separati dalle tacche, nessuna sovrapposizione, trasparenze controllate, geometria HSI circolare e dati NAV chiaramente identificati. Catture Windows o concept non valgono come acceptance Linux/ARM64 o pannello reale.")
    sub("Terreno reale","Pacchetti DEM Copernicus/SRTM e texture Sentinel-2, GPU/LOD/cache e route Treviso-Belluno/Dolomiti. La copertura e il datum devono essere coerenti con pista, posizione e ostacoli. Immagini satellitari, DEM, vettori e dati aeronautici hanno ruoli diversi; OpenAIP non e' una texture del terreno.")
    sub("Lavoro residuo","Gap tile registrati, dettaglio vicino e pianura, seamless streaming, frame time e memoria, allineamento pista, ostacoli con fonte/eta'/datum, TAWS qualificato e acceptance golden. Il touch PFD resta un difetto aperto nell'ultima prova live; le prove positive MFD storiche non lo chiudono.")
    table(["Gate","Criterio richiesto"],[("Visuale","Stessa release sul pannello target, confronto approvato senza cambiare tolleranze per fare passare il test."),
        ("Prestazioni","Carico reale, frametime p95, FPS, RAM/GPU, temperatura e throttling; il valore medio da solo non basta."),
        ("Sicurezza dati","Terreno mancante non rappresentato come sicuro; posizione invalida e dati stale chiaramente annunciati.")],[100,399])

    section("07 / EMS, sensori e impianti")
    sub("Interfaccia e workflow","EMS dedicato RPi4 con circolari graduati, barre CHT/EGT, fuel, elettrico, carrello/flap configurabili e unita'. PFD/Canvas espongono la vista compatta comune. Setup motore, calibrazione, logging, ore e simulatori sono nel perimetro; una UI completa non prova la misura reale.")
    sub("Catena sensori reale","Hardware fotografato Sequent: ingressi analogico/digitali, termocoppie e ingressi LV/HV. Serve inventario di ogni scheda/revisione, alimentazione, DIP, interfaccia effettiva e cablaggio prima di driver o scansioni invasive. Il nodo sensor RPi4 resta distinto dall'EMS display.")
    sub("Calibrazione e validazione","Mappa per sorgente/nodo/canale, raw e engineering value, tipo sonda, gain/offset e revisione. Prove con riferimento indipendente, open/short, timeout, plausibilita' e recovery. Non utilizzare gli ingressi HV come consiglio di cablaggio: il dossier non e' uno schema elettrico operativo.")
    sub("Modelli motore","Rotax 912 UL e 912 ULS devono conservare identita', limiti e manuali distinti. I valori non approvati restano SIM o non disponibili. Le ore SIM non devono incrementare i contatori operativi; RPM e condizioni di conteggio richiedono regole documentate e verifica reale.")
    sub("Impianto di bordo","Target: bus/batteria/alternatore, pompe, utenze motore, interruttori, protezioni e stato reale contro comando. Il disegno del sinottico deve derivare dall'impianto del singolo aereo, non da un layout generico. Allarmi richiedono limiti approvati, persistenza, latch/ack e priorita'.")

    section("08 / Navigazione, meteo e dati geografici")
    table(["Dominio","Gia' presente","Da chiudere"],[
        ("NAV","Database aeroporti/navaid, pagine NAV e prototipi GPS/VOR/NDB, route e selezione aeroporti.","FlightPlan canonico, leg/OBS/CDI/BRG/XTK/DTK, identita' e validita', direct-to/alternati, sorgenti reali e failover."),
        ("Meteo","METAR/TAF/radar, parser/cache/freshness e correzioni asincrone software.","Provider/TLS/georeferenziazione, aging separato, cache sul target senza stall; prodotti SIL nei veri consumer."),
        ("Geografia","DEM/texture/cache e pacchetti per area/corridoio nel progetto.","Catalogo Nord Italia, quote disco e download riprendibile, destinazione/alternati, offline e copertura verificata."),
        ("Licenze","Provenienza OSM/OpenAIP/Copernicus registrata nei moduli/dati.","Verifica per ogni dataset e uso previsto; non assumere che dati non-commerciali siano liberamente riutilizzabili nel prodotto.")],[70,211,218])
    sub("Regole di qualita'","Conservare tempi di osservazione, validita', acquisizione e clock skew. LIVE/CACHED/STALE/SIM devono essere inequivocabili. Un radar georeferenziato male e' un dato falso anche se bello; TAF sintetico persistence-only non e' una previsione operativa.")
    sub("Radio e aeroporto","L'aeroporto selezionato non e' automaticamente utilizzabile o raggiungibile. Pista, soglie, quota, orientamento, fonte e applicabilita' devono essere coerenti. Nessuna trasmissione radio o selezione automatica di apparati e' autorizzata dalla pagina NAV.")

    section("09 / Backbone, rete e registrazione")
    sub("Responsabilita' della Blackbox","Ingestion read-only, normalizzazione/arbitraggio, health, distribuzione e registrazione. DDP trasporta lo stato di volo, EDP quello motore; NAV/WX/airframe richiedono oggetti e adapter espliciti, non campi sovraccaricati. I display non ricevono responsabilita' di decodificare bus sensori.")
    table(["Dominio","Disegno e confine"],[("FLIGHT-DATA","10.77.10.0/24: Pixhawk, Blackbox, PFD, EMS, MFD. Indirizzi e porte di progetto non attestano tutte le attivazioni attuali."),
        ("VIDEO","10.77.20.0/24 fisicamente distinto. Quattro camere e RPi4 VIDEO-CORE; niente concentrazione sullo switch avionico a cinque porte."),
        ("Servizio","192.168.1.0/24 per manutenzione/server/NAS. Nessun bridge/NAT/IP forwarding verso FLIGHT-DATA."),
        ("Osservatori","Copia MAVLink solo in uscita verso endpoint nominati; nessun percorso di ritorno verso Pixhawk."),
        ("Switch","Waveshare Gigabit-Switch-5P identificato per banco; insufficiente alla configurazione finale. MokerLink candidato non qualificato, non sostituzione gia' approvata.")],[98,401])
    sub("Registrazione operativa contro registro SIL","Blackbox/recorder di prodotto, spool consultivo e truth journal SIL sono tre oggetti diversi. R1-25 async-test non ha sostituito il recorder operativo. Un ACK di accodamento non e' durabilita'; completion richiede flush/hash. Restano power-loss, directory fsync, recovery e endurance.")

    section("10 / FPV, voce ed emergenze")
    sub("FPV e telemetria","Restano requisiti del progetto. Il target e' quattro stream, selezione/mosaico, telemetria coerente, health/latency e fallback. RPi4 EMS e RPi4 FPV non possono essere lo stesso host. Camere, ottiche/gimbal, decoder e vibrazioni richiedono scelta e prove reali; non sono completati dalle pagine demo.")
    sub("Voce naturale e riconoscimento","Architettura consultiva offline, PTT e grammatica chiusa. Annunci critici registrati/versionati e TTS neurale per dinamici non critici sono il target; microfono/interfono, priorita' STALL/TAWS, rumore e falsi comandi sono da collaudare. QGC/portatile non sono il nodo audio EFIS.")
    sub("Radio Assist IT/EN","Template distinti e versionati, con giurisdizione, fonte, applicabilita' e valori correnti. Nessuna fraseologia operativa generata liberamente da AI; bozze sottoposte al pilota e transmit_allowed=false. Dire FATTO registra acknowledgement, non verifica un sensore.")
    sub("Emergency Assist / panico","Assetto, informazioni utili, checklist e candidati sono assistenza consultiva. Dati e modello simulati producono SIM_ESTIMATE, non REACHABLE operativo. Checklist devono provenire da package approvato. PAN-002 e PAN-003 restano BLOCKED: servi rollio/beccheggio, stabilize/glide/autoland richiedono nuova autorita', safety case e prove HIL separate.")

    section("11 / Profili, setup e personalizzazione")
    sub("Aeromobile e singolo esemplare","Catalogo comune aggiornabile, staging, validazione e attivazione atomica del profilo. Il database operativo esteso e' documentato con 447 parametri in 28 domini, ma completezza dello schema non equivale a dati approvati di un singolo aereo. Ogni valore critico richiede fonte, revisione/pagina e applicabilita'.")
    sub("Standardizzazione","Unita' di misura e conversioni coerenti; mappa sensori, motore, elettrico, carrello/flap e capability esplicite. Configurazioni mancanti non vanno compensate con assunzioni. Le pagine di setup condividono logica e componenti senza fork RPi4/RPi5.")
    sub("Workflow atteso","Seleziona modello -> personalizza esemplare -> revisione delle fonti -> validazione -> staging -> attivazione con audit/rollback. Setup critico a terra; prove SIM e dati indicativi separati dai parametri operativi.")
    sub("Regola su ASSO e Pioneer","La somiglianza meccanica non autorizza equivalenza di prestazioni, masse, V-speeds, limiti o procedure. ASSO 5 Jolly e Pioneer 300 sono profili distinti finche' una verifica documentale e tecnica specifica non sostiene la parte riutilizzabile.")

    section("12 / Simulatore: capacita' realmente provate")
    text("Il SIL usa serverha e deve alimentare le vere applicazioni EFIS. JSBSim 1.3.3/NumPy 2.3.4 sono isolati e fissati; C172P e' REFERENCE_UNVALIDATED. Windows/FS2024 sono opzionali, non la dipendenza del motore autonomo. Il bridge MSFS finale non viene considerato qualificato.")
    table(["Blocco","Evidenza e limite"],[("Fisica/sessione","100Hz integrazione e20Hz snapshot; scheduler indipendente, clock monotono, pause/epoch, stop su overrun oltre100ms. Non validazione delle prestazioni ASSO/Rotax."),
        ("Mondo","Vento/atmosfera nella fisica; NAV/radio/GNSS/route e radar/meteo analitici SIM. Prodotti applicativi e consumer grafici completi ancora da collegare."),
        ("Comandi","Coda bounded, lease e dedup; fixture locale di tre ingressi FDM, audit e readback. Massimo16comandi nel primo secondo, cruise senza pausa: NON console live completa."),
        ("Registro","Async-test opt-in,16record/512KiB,ACK durabilita' entro100ms. Un disco di sistema ha fallito; run60s sul secondoSSD passate. Non endurance o qualifica power-loss."),
        ("Archivi","Pack/restore bounded con manifesti/hash e semantica trace; run60s ripristinata server/Windows con identici dati. Non restore runtime/modelli o NAS."),
        ("Display","Prove PFD live storiche su sottoinsieme dati con helper ristretto e restore. Il nuovo producer non e' ancora un fanout simultaneo PFD/MFD/EMS.")],[99,400])
    ev=snapshot["evidence"]
    text(f"Ultima evidenza R1-28: {ev['local_tests']['total_methods']} test Python locali e {ev['server_tests']['methods']} server PASS, inclusi {ev['server_tests']['native_jsbsim_methods']} nativi JSBSim. CTest locale 11/12 PASS: una suite nativa bloccata da WinError4551; nessuna policy aggirata. Non chiamare questa suite interamente verde.")

    section("13 / Simulatore: pacchetti e uscita")
    table(["ID / stato","Pacchetto","Criterio di uscita ancora da dimostrare"],[(x["id"]+" / "+x["state"],x["title"],x["exit"]) for x in snapshot["simulation"]["packages"]],[91,180,228])
    text("Tutti i criteri sono necessari nel loro ambito: nessun DONE artificiale. P03 e P07 sono PARTIAL, non piu' semplici intenzioni, ma console live e replica NAS restano mancanti. Il simulatore nel complesso e' NOT_COMPLETE.")

    section("14 / Rischi e gate di sicurezza")
    table(["Priorita'","Rischio","Azione prima della chiusura"],[
        ("Critica","Confusione dati SIM/reali o canale mancante mostrato valido","Provenance, source arbitration, null/STALE/LOST e test end-to-end con consumer reali."),
        ("Critica","Comandi/servi introdotti dal percorso consultivo","Mantenere assenza di ritorno verso Pixhawk; nuova ADR e safety case separati per attuazione."),
        ("Alta","Alimentazione MFD/Blackbox e guasto comune","Inventario PSU/cavo, cold/warm boot, carico e soak; 0x50000 storico non prova guasto attivo ma non chiude il gate."),
        ("Alta","Limiti aereo/motore o modello non verificati","Manuali dell'esemplare, UL/ULS distinti, calibrazioni e envelope indipendente."),
        ("Alta","Grafica/terreno poco leggibili o incoerenti","Golden Linux non derogata; acceptance sul target, misure frametime e gestione dei gap."),
        ("Alta","Storage/Blackbox, perdita dati e falsa completion","Endurance/power-loss/recovery; distinguere accodato,durevole e completo."),
        ("Alta","Video o voce saturano funzione primaria","Nodi/reti/quote risorse distinti e fallback; prove fault/full-load."),
        ("Media","CI verde scambiata per deployment","Separare commit,PR,build,candidata,release attiva e osservazione hardware datata."),
        ("Media","Licenze/dataset errati o obsoleti","Catalogo con diritti,origine,revisione,coverage,datum e validita'; no asset proprietari copiati senza diritti.")],[58,179,262])

    section("15 / Planning operativo di completamento")
    text("Pianificazione per gate, non per numero di passi eseguiti. Un gate hardware e un blocco software indipendente possono avanzare senza confondere le rispettive prove. Nessuna data di chiusura inventata senza inventario e baseline.")
    table(["Ordine","Consegna","Autonomo software / intervento umano","Uscita"],[
        ("1","Baseline e test","Code review,CI,stato/repliche; sicurezza Windows da gestire senza bypass.","HEAD/evidenze coerenti;blocchi dichiarati."),
        ("2","SIL pilotabile","Console/inbox,capability/lifecycle,audit/replay;poi prove su banco isolato.","Comandi e rollback verificabili lungo missione."),
        ("3","SIL distribuito","Broker/codec EDP-DDP e prodotti,consumer headless;hardware solo dopo gate.","Stessa sessione su PFD/MFD/EMS,senza generatori concorrenti."),
        ("4","Impianti e dati","Modelli/sonde/WX/NAV/catalogo;manuali e misure per validazione reale.","Conservazione,guasti e recovery con dati applicabili."),
        ("5","Power/display/sensori","Richiede operatore:PSU,cablaggi,inventario,calibrazione,touch/golden.","Accettazione della release specifica sui target."),
        ("6","Video/audio/emergenze","Fixture software;poi telecamere,PTT/interfono e rumore reale.","Latenza/priorita'/fallback e confini consultivi provati."),
        ("7","Prodotto e HIL","Restore NAS,soak,power-loss,EMI/vibrazioni e piano approvato.","Acceptance banco documentata;volo resta fase separata.")],[39,96,227,137])

    section("16 / Attivita' interattive e accettazione")
    table(["Prova da eseguire con operatore","Evidenza necessaria"],[
        ("PSU/cavi e cold/warm boot MFD/Blackbox","Foto targhette,misure sotto carico,log kernel,throttled e temperatura;nessun reset/dato perso."),
        ("Touch PFD e leggibilita'","Stessa release,coordinate/eventi,tocchi MENU/NAV/setup;foto/cattura confrontata al riferimento."),
        ("Schede motore e cablaggio","Inventario fronte/retro,revisione,DIP,alimentazione,canali,isolamento e riferimento indipendente."),
        ("Calibrazione sonde e limiti","Raw/engineering,curve e revisioni,open/short/fault,manuale dell'esemplare."),
        ("GNSS e pitot/airspeed","Fix/accuratezza,leak test e zero,confronto indipendente;nessuna variazione ARSPD_USE implicita."),
        ("Switch/rete/video","Port map,link speed,jitter,banda,isolamento fisico,fault/recovery e latenza quattro camere."),
        ("Audio/voce","PTT,rumore cabina,intelligibilita',falsi positivi/negativi e priorita' allarmi."),
        ("Power-loss e restore","Piano autorizzato,copie preservate,checksum,recovery e verifica su storage di destinazione."),
        ("HIL/attuazione futura","Safety case,fixture e autorita' esplicite;non parte di queste attivita' software.")],[222,277])
    text("Non servono interventi manuali per documentazione,validatori,test sintetici e molte prove isolate su server. Non e' lecito trasformare quelle prove in collaudo dei dispositivi reali.")

    section("17 / Repository, rilascio e controllo progetto")
    sub("Fonti autorevoli","GitHub efis-next-professional e' il repository condiviso; efis-evolved resta storico separato. Serverha ospita mirror/working copy; NAS conserva archivio,Blackbox copia ridondante storica. Lo stato attuale delle repliche deve essere verificato,non dedotto da un push GitHub.")
    sub("Release","Una PR o un commit non e' una release attiva. Manifesto,hash,binario canonico,rollback,canary e reboot acceptance sono gate della specifica attivazione. Main,branch di lavoro e candidati restano distinti; questo dossier non autorizza merge o rollout.")
    sub("Cruscotto interattivo","Matrice -> generate_status.py -> report JSON/Markdown; generate_dashboard.py -> viewer filtrabile. Esiste pubblicazione serverha read-only con aggiornamento da main,storicamente verificata;non e' monitoraggio live dei sensori e non mostra automaticamente un branch non integrato. Freshness della vista e CI sono indicatori separati.")
    sub("Collaborazione Codex / ChatGPT / Claude","Codice,evidenze,ADR e stato versionati sono il passaggio di consegne. La chat serve per decisioni/requisiti,non sostituisce il repository. Ogni ripresa indica branch/HEAD,ID,limiti e prove. Non esiste sincronizzazione magica delle conversazioni fra agenti.")

    section("18 / Fonti e riproducibilita'")
    text("Fonti primarie interne del dossier. Le descrizioni architetturali sono disegni/decisioni; le date e gli scope delle evidenze decidono cosa e' stato realmente provato. Nessun sito web o stato commerciale e' stato usato per aggiornare questo snapshot.")
    for source in snapshot["sources"]:
        story.extend([p(source["path"],"H2D"),p("SHA256 "+source["sha256"],"SmallD")])
    text("La sintesi e i conteggi sono generati dalla matrice,non ricopiati a mano. Il PDF e' rigenerabile;il JSON di snapshot lega i dati alla revisione consultata. La mappa mostra funzioni e responsabilita',non uno schema esecutivo di cablaggio o una dichiarazione di certificazione.")

    section("Appendice / Tutti i requisiti censiti")
    text("Per ogni requisito:stato,evidenza sintetica,rischio,prossimo passo e criterio di chiusura. L'evidenza storica molto lunga e' abbreviata esplicitamente dopo650caratteri;la riga completa e' nella matrice e nello snapshot JSON. Nessuna voce e' stata omessa.")
    for row in snapshot["requirements"]:
        evidence=row["evidence"]
        if len(evidence)>650:evidence=evidence[:650]+" ... [evidenza estesa nella matrice e nello snapshot JSON]"
        story.append(KeepTogether([p(row["id"]+" / "+row["requirement"],"H2D"),
            p("STATO: "+row["status"]+" | NODO: "+row["node"],"LabelD"),
            p("Evidenza: "+evidence,"SmallD"),p("Rischi/aperto: "+row["risk"],"SmallD"),
            p("Prossimo passo: "+row["next_step"],"SmallD"),p("Chiusura: "+row["acceptance"],"SmallD"),Spacer(1,7)]))

    def footer(cv,doc):
        w,h=A4;cv.setStrokeColor(colors.HexColor("#D4E0E7"));cv.line(48,42,w-48,42)
        cv.setFont("Dossier",7);cv.setFillColor(gray)
        cv.drawString(48,29,"FUSO AVIONICS NEXT | 28.10.2026 | DEVELOPMENT / NON-CERT")
        cv.drawRightString(w-48,29,f"{doc.page}")
    output=Path(output);output.parent.mkdir(parents=True,exist_ok=True)
    class DossierDoc(SimpleDocTemplate):
        def afterFlowable(self,flow):
            if isinstance(flow,Paragraph) and flow.style.name=="H1D":
                key="section-"+str(self.seq.nextf("outline"))
                self.canv.bookmarkPage(key)
                self.canv.addOutlineEntry(flow.getPlainText(),key,level=0)
    doc=DossierDoc(str(output),pagesize=A4,rightMargin=48,leftMargin=48,
        topMargin=42,bottomMargin=57,title="Fuso Avionics Next - progetto e stato di avanzamento",
        author="Fuso Avionics Next",pageCompression=1)
    doc.build(story,onFirstPage=footer,onLaterPages=footer)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output",type=Path,required=True)
    parser.add_argument("--snapshot",type=Path,required=True)
    args=parser.parse_args()
    snapshot=collect()
    build(args.output,snapshot)
    args.snapshot.parent.mkdir(parents=True,exist_ok=True)
    args.snapshot.write_text(json.dumps(snapshot,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print(json.dumps(dict(status="CREATED_REQUIRES_VISUAL_QA",requirements=len(snapshot["requirements"]),
                         output=str(args.output),head=snapshot["head"])))


if __name__=="__main__":main()
