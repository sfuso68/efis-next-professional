import QtQuick

Item {
    id: root
    required property var controller
    property real uiScale: 1
    property string editDisposition: "UNKNOWN"
    property int workflowPage: 0
    readonly property var profiles: controller.operationalProfiles || []
    readonly property var domains: controller.operationalDomains || []
    readonly property var fields: controller.operationalFields || []
    readonly property var summary: controller.operationalSummary || ({})
    readonly property var selected: controller.operationalSelectedField || ({})
    readonly property var domainSummaries: controller.operationalDomainSummaries || []
    readonly property var validationIssues: controller.operationalValidationIssues || []
    readonly property var validationReport: controller.operationalValidationReport || ({})
    readonly property color cyan: "#28D9F4"
    readonly property color green: "#52DE36"
    readonly property color amber: "#FFC72B"
    readonly property color red: "#FF5050"
    readonly property color line: "#40535E"
    readonly property color panel: "#E00A1821"

    function syncEditor(field) {
        if (!field || field.key === undefined) {
            editDisposition = "UNKNOWN"
            valueInput.text = ""
            rationaleInput.text = ""
            sourceInput.text = ""
            revisionInput.text = ""
            pageInput.text = ""
            return
        }
        editDisposition = String(field.disposition || "UNKNOWN")
        valueInput.text = editDisposition !== "VALUE" || field.value === undefined ? "" :
            (typeof field.value === "object" ? JSON.stringify(field.value) : String(field.value))
        rationaleInput.text = String(field.rationale || "")
        sourceInput.text = String(field.source_document || "")
        revisionInput.text = String(field.source_revision || "")
        pageInput.text = String(field.source_page || "")
    }
    function chooseField(field) {
        controller.selectOperationalField(String(field.key))
    }
    onSelectedChanged: syncEditor(selected)
    Component.onCompleted: {
        var arguments = Qt.application.arguments
        if (arguments.indexOf("--setup-operational-entry-preview") >= 0) workflowPage = 1
        else if (arguments.indexOf("--setup-operational-load-preview") >= 0) workflowPage = 2
        else if (arguments.indexOf("--setup-operational-check-preview") >= 0) workflowPage = 3
        syncEditor(selected)
    }
    function cycleProfile() {
        if (!profiles.length) return
        var current = String(controller.operationalSelectedProfile || "")
        var index = 0
        for (var i=0; i<profiles.length; ++i) if (String(profiles[i].profile_key) === current) index=i
        controller.selectOperationalProfile(String(profiles[(index+1)%profiles.length].profile_key))
    }

    Rectangle { anchors.fill:parent; color:"transparent"
        Rectangle { id:summaryBar; anchors { left:parent.left; right:parent.right; top:parent.top } height:58*root.uiScale; color:root.panel; border.color:root.line
            Text { x:10*root.uiScale; y:7*root.uiScale; text:"CONTROLLED AIRCRAFT PROFILE"; color:root.cyan; font.pixelSize:8*root.uiScale; font.bold:true }
            Text { x:10*root.uiScale; y:25*root.uiScale; width:365*root.uiScale; elide:Text.ElideRight; text:String(controller.operationalSelectedProfile||"NO PROFILE")+"  ›"; color:"white"; font.pixelSize:11*root.uiScale; font.bold:true
                TapHandler { onTapped:root.cycleProfile() }
            }
            Row { anchors { right:parent.right; rightMargin:10*root.uiScale; verticalCenter:parent.verticalCenter } spacing:7*root.uiScale
                Repeater { model:[
                    {k:"FIELDS",v:Number(root.summary.total||0),c:root.cyan},
                    {k:"UNKNOWN",v:Number(root.summary.unresolved||0),c:root.amber},
                    {k:"SAFETY BLOCK",v:Number(root.summary.safety_blockers||0),c:root.red},
                    {k:"SIMULATED",v:Number(root.summary.simulated||0),c:root.red},
                    {k:"EDITED",v:Number(root.summary.edited||0),c:root.green}]
                    delegate:Rectangle { required property var modelData; width:69*root.uiScale; height:42*root.uiScale; radius:3*root.uiScale; color:"#102630"; border.color:modelData.c
                        Text { anchors { horizontalCenter:parent.horizontalCenter; top:parent.top; topMargin:4*root.uiScale } text:modelData.k; color:modelData.c; font.pixelSize:5.5*root.uiScale; font.bold:true }
                        Text { anchors { horizontalCenter:parent.horizontalCenter; bottom:parent.bottom; bottomMargin:3*root.uiScale } text:modelData.v; color:"white"; font.pixelSize:14*root.uiScale; font.bold:true }
                    }
                }
            }
        }

        Rectangle { id:workflowBar; anchors { left:parent.left; right:parent.right; top:summaryBar.bottom } height:34*root.uiScale; color:"#07141C"; border.color:root.line
            Row { anchors.centerIn:parent; spacing:6*root.uiScale
                Repeater { model:["OVERVIEW","GUIDED ENTRY","LOAD / PREFILL","CHECK & REVIEW"]
                    delegate:Rectangle { required property string modelData; required property int index; width:142*root.uiScale; height:25*root.uiScale; radius:3*root.uiScale; color:root.workflowPage===index?"#174052":"#0D2029"; border.color:root.workflowPage===index?root.cyan:root.line
                        Text { anchors.centerIn:parent; text:modelData; color:root.workflowPage===index?"white":"#91A3AB"; font.pixelSize:6.5*root.uiScale; font.bold:true }
                        TapHandler { onTapped:root.workflowPage=index }
                    }
                }
            }
        }

        Rectangle { id:domainPane; visible:root.workflowPage===1; anchors { left:parent.left; top:workflowBar.bottom; bottom:statusBar.top; topMargin:7*root.uiScale } width:154*root.uiScale; color:root.panel; border.color:root.line
            Text { x:9*root.uiScale; y:7*root.uiScale; text:"DOMAINS • 28"; color:root.cyan; font.pixelSize:7*root.uiScale; font.bold:true }
            ListView { anchors { left:parent.left; right:parent.right; top:parent.top; bottom:parent.bottom; topMargin:25*root.uiScale; margins:5*root.uiScale } clip:true; spacing:2*root.uiScale; model:root.domains
                delegate:Rectangle { required property var modelData; width:ListView.view.width; height:29*root.uiScale; color:String(controller.operationalSelectedDomain)===String(modelData.id)?"#174052":"transparent"; border.color:String(controller.operationalSelectedDomain)===String(modelData.id)?root.cyan:"transparent"
                    Text { x:7*root.uiScale; anchors.verticalCenter:parent.verticalCenter; width:112*root.uiScale; elide:Text.ElideRight; text:String(modelData.label||modelData.id).toUpperCase(); color:String(controller.operationalSelectedDomain)===String(modelData.id)?"white":"#AAB6BC"; font.pixelSize:6.5*root.uiScale; font.bold:true }
                    Text { anchors { right:parent.right; rightMargin:6*root.uiScale; verticalCenter:parent.verticalCenter } text:String(modelData.parameter_count||0); color:root.cyan; font.pixelSize:6*root.uiScale }
                    TapHandler { onTapped:controller.selectOperationalDomain(String(modelData.id)) }
                }
            }
        }

        Rectangle { id:listPane; visible:root.workflowPage===1; anchors { left:domainPane.right; top:domainPane.top; bottom:domainPane.bottom; leftMargin:7*root.uiScale } width:292*root.uiScale; color:root.panel; border.color:root.line
            Rectangle { x:7*root.uiScale; y:7*root.uiScale; width:parent.width-14*root.uiScale; height:29*root.uiScale; radius:3*root.uiScale; color:"#102630"; border.color:root.line
                TextInput { id:searchInput; anchors { fill:parent; margins:6*root.uiScale } color:"white"; font.pixelSize:7*root.uiScale; clip:true; selectByMouse:true; text:""; onTextEdited:controller.filterOperationalFields(text) }
                Text { visible:!searchInput.text.length; x:6*root.uiScale; anchors.verticalCenter:parent.verticalCenter; text:"SEARCH FIELD / IDENTIFIER"; color:"#70838C"; font.pixelSize:7*root.uiScale }
            }
            ListView { anchors { left:parent.left; right:parent.right; top:parent.top; bottom:parent.bottom; topMargin:43*root.uiScale; margins:6*root.uiScale } clip:true; spacing:3*root.uiScale; model:root.fields
                delegate:Rectangle { required property var modelData; required property int index; width:ListView.view.width; height:48*root.uiScale; color:String(root.selected.key)===String(modelData.key)?"#163847":(index%2?"#0B1922":"#10222C"); border.color:modelData.blocker?root.red:(modelData.unresolved?root.amber:"transparent")
                    Rectangle { width:4*root.uiScale; height:parent.height; color:modelData.blocker?root.red:(modelData.edited?root.green:"transparent") }
                    Text { x:9*root.uiScale; y:5*root.uiScale; width:parent.width-18*root.uiScale; elide:Text.ElideRight; text:String(modelData.label||""); color:"white"; font.pixelSize:7*root.uiScale; font.bold:true }
                    Text { x:9*root.uiScale; y:22*root.uiScale; width:parent.width-92*root.uiScale; elide:Text.ElideRight; text:String(modelData.display_value||"—")+(modelData.canonical_unit?" "+String(modelData.canonical_unit):""); color:modelData.unresolved?root.amber:root.cyan; font.pixelSize:7*root.uiScale }
                    Row { anchors { right:parent.right; rightMargin:6*root.uiScale; bottom:parent.bottom; bottomMargin:6*root.uiScale } spacing:4*root.uiScale
                        Text { text:modelData.effective_simulated?"SIM":""; color:root.red; font.pixelSize:5.5*root.uiScale; font.bold:true }
                        Text { text:String(modelData.criticality)==="SAFETY_CRITICAL"?"SAFETY":""; color:root.red; font.pixelSize:5.5*root.uiScale; font.bold:true }
                    }
                    TapHandler { onTapped:root.chooseField(modelData) }
                }
            }
        }

        Rectangle { id:editor; visible:root.workflowPage===1; anchors { left:listPane.right; right:parent.right; top:domainPane.top; bottom:domainPane.bottom; leftMargin:7*root.uiScale } color:root.panel; border.color:root.selected.blocker?root.red:root.line
            Text { x:10*root.uiScale; y:8*root.uiScale; width:parent.width-20*root.uiScale; elide:Text.ElideRight; text:String(root.selected.label||"SELECT A FIELD"); color:"white"; font.pixelSize:10*root.uiScale; font.bold:true }
            Text { x:10*root.uiScale; y:27*root.uiScale; width:parent.width-20*root.uiScale; elide:Text.ElideMiddle; text:String(root.selected.key||""); color:root.cyan; font.pixelSize:6*root.uiScale }
            Text { x:10*root.uiScale; y:43*root.uiScale; text:String(root.selected.value_type||"—")+(root.selected.canonical_unit?" • "+String(root.selected.canonical_unit):"")+" • "+String(root.selected.requirement||""); color:"#AAB6BC"; font.pixelSize:6*root.uiScale }
            Rectangle { visible:root.selected.effective_simulated===true; anchors { right:parent.right; rightMargin:10*root.uiScale; top:parent.top; topMargin:8*root.uiScale } width:96*root.uiScale; height:25*root.uiScale; radius:2*root.uiScale; color:"#4A1117"; border.color:root.red
                Text { anchors.centerIn:parent; text:"SIMULATED"; color:"white"; font.pixelSize:6.5*root.uiScale; font.bold:true }
            }

            Grid { x:10*root.uiScale; y:58*root.uiScale; columns:3; spacing:3*root.uiScale
                Repeater { model:[{l:"VALUE",v:"VALUE"},{l:"UNKNOWN",v:"UNKNOWN"},{l:"INHERIT",v:"INHERIT"},{l:"N/A",v:"NOT_APPLICABLE"},{l:"NOT INST.",v:"NOT_INSTALLED"}]
                    delegate:Rectangle { required property var modelData; width:83*root.uiScale; height:21*root.uiScale; radius:2*root.uiScale; color:root.editDisposition===modelData.v?"#1A4857":"#102630"; border.color:root.editDisposition===modelData.v?root.cyan:root.line
                        Text { anchors.centerIn:parent; text:modelData.l; color:root.editDisposition===modelData.v?"white":"#9EADB4"; font.pixelSize:5.5*root.uiScale; font.bold:true }
                        TapHandler { onTapped:root.editDisposition=modelData.v }
                    }
                }
            }

            component FieldBox: Rectangle {
                property alias text: input.text
                property string hint: ""
                width:parent.width-20*root.uiScale; height:25*root.uiScale; radius:2*root.uiScale; color:"#0B1820"; border.color:root.line
                TextInput { id:input; anchors { fill:parent; margins:5*root.uiScale } color:"white"; font.pixelSize:6.5*root.uiScale; clip:true; selectByMouse:true }
                Text { visible:!input.text.length; x:6*root.uiScale; anchors.verticalCenter:parent.verticalCenter; text:parent.hint; color:"#647881"; font.pixelSize:6*root.uiScale }
            }
            FieldBox { id:valueInput; x:10*root.uiScale; y:106*root.uiScale; hint:"VALUE • JSON FOR TABLE/CURVE" }
            FieldBox { id:sourceInput; x:10*root.uiScale; y:134*root.uiScale; hint:"SOURCE DOCUMENT" }
            Row { x:10*root.uiScale; y:162*root.uiScale; spacing:5*root.uiScale
                FieldBox { id:revisionInput; width:125*root.uiScale; hint:"REVISION" }
                FieldBox { id:pageInput; width:125*root.uiScale; hint:"PAGE" }
            }
            FieldBox { id:rationaleInput; x:10*root.uiScale; y:190*root.uiScale; hint:"RATIONALE / APPLICABILITY" }

            Rectangle { x:10*root.uiScale; y:220*root.uiScale; width:161*root.uiScale; height:30*root.uiScale; radius:3*root.uiScale; color:"#154234"; border.color:root.green
                Text { anchors.centerIn:parent; text:"SAVE DRAFT FIELD"; color:"white"; font.pixelSize:7*root.uiScale; font.bold:true }
                TapHandler { onTapped:controller.stageOperationalField(root.editDisposition,valueInput.text,rationaleInput.text,sourceInput.text,revisionInput.text,pageInput.text) }
            }
            Rectangle { x:178*root.uiScale; y:220*root.uiScale; width:82*root.uiScale; height:30*root.uiScale; radius:3*root.uiScale; color:"#3A211B"; border.color:root.amber
                Text { anchors.centerIn:parent; horizontalAlignment:Text.AlignHCenter; text:"HOLD 1.2s\nCLEAR DRAFT"; color:root.amber; font.pixelSize:5.5*root.uiScale; font.bold:true }
                TapHandler { longPressThreshold:1.2; onLongPressed:controller.discardOperationalDraft() }
            }
            Rectangle { x:10*root.uiScale; y:255*root.uiScale; width:250*root.uiScale; height:25*root.uiScale; radius:3*root.uiScale; color:"#102D3A"; border.color:root.cyan
                Text { anchors.centerIn:parent; text:"SAVE AND OPEN NEXT ISSUE"; color:root.cyan; font.pixelSize:6.5*root.uiScale; font.bold:true }
                TapHandler { onTapped:if(controller.stageOperationalField(root.editDisposition,valueInput.text,rationaleInput.text,sourceInput.text,revisionInput.text,pageInput.text))controller.selectNextOperationalIssue() }
            }
            Text { x:10*root.uiScale; y:284*root.uiScale; width:parent.width-20*root.uiScale; height:27*root.uiScale; maximumLineCount:2; elide:Text.ElideRight; wrapMode:Text.WordWrap; text:"Safety-critical VALUE fields require controlled document, revision and page. This screen cannot activate a profile."; color:root.amber; font.pixelSize:5.5*root.uiScale }
        }

        Item { id:overviewPage; visible:root.workflowPage===0; anchors { left:parent.left; right:parent.right; top:workflowBar.bottom; bottom:statusBar.top; margins:7*root.uiScale }
            Rectangle { id:profilePanel; anchors { left:parent.left; top:parent.top; bottom:parent.bottom } width:292*root.uiScale; color:root.panel; border.color:root.line
                Text { x:12*root.uiScale; y:10*root.uiScale; text:"1 • CHOOSE WORKING PROFILE"; color:root.cyan; font.pixelSize:8*root.uiScale; font.bold:true }
                Text { x:12*root.uiScale; y:29*root.uiScale; width:parent.width-24*root.uiScale; wrapMode:Text.WordWrap; text:"Installed aircraft remains separate from the complete SIM scenario."; color:"#9AABB3"; font.pixelSize:6*root.uiScale }
                ListView { anchors { left:parent.left; right:parent.right; top:parent.top; bottom:parent.bottom; margins:10*root.uiScale; topMargin:66*root.uiScale } spacing:6*root.uiScale; clip:true; model:root.profiles
                    delegate:Rectangle { required property var modelData; width:ListView.view.width; height:72*root.uiScale; radius:3*root.uiScale; color:String(controller.operationalSelectedProfile)===String(modelData.profile_key)?"#173B49":"#0D2029"; border.color:String(modelData.profile_key).indexOf("simulation.")===0?root.red:(String(controller.operationalSelectedProfile)===String(modelData.profile_key)?root.cyan:root.line)
                        Text { x:9*root.uiScale; y:8*root.uiScale; width:parent.width-18*root.uiScale; elide:Text.ElideRight; text:String(modelData.title||modelData.profile_key); color:"white"; font.pixelSize:7*root.uiScale; font.bold:true }
                        Text { x:9*root.uiScale; y:28*root.uiScale; text:String(modelData.scope_type||"")+" • "+String(modelData.lifecycle_state||""); color:String(modelData.profile_key).indexOf("simulation.")===0?root.red:root.cyan; font.pixelSize:6*root.uiScale }
                        Text { x:9*root.uiScale; y:47*root.uiScale; text:String(modelData.validation&&modelData.validation.release_ready?"RELEASE READY":"DRAFT / NOT ACTIVE"); color:root.amber; font.pixelSize:6*root.uiScale; font.bold:true }
                        TapHandler { onTapped:controller.selectOperationalProfile(String(modelData.profile_key)) }
                    }
                }
            }
            Rectangle { anchors { left:profilePanel.right; right:parent.right; top:parent.top; bottom:parent.bottom; leftMargin:7*root.uiScale } color:root.panel; border.color:root.line
                Text { x:12*root.uiScale; y:10*root.uiScale; text:"2 • COMPLETE BY SECTION"; color:root.cyan; font.pixelSize:8*root.uiScale; font.bold:true }
                Text { anchors { right:parent.right; rightMargin:12*root.uiScale; top:parent.top; topMargin:10*root.uiScale } text:"TOUCH A SECTION TO CONTINUE"; color:"#91A3AB"; font.pixelSize:6*root.uiScale }
                GridView { anchors { fill:parent; margins:10*root.uiScale; topMargin:35*root.uiScale } clip:true; cellWidth:166*root.uiScale; cellHeight:52*root.uiScale; model:root.domainSummaries
                    delegate:Rectangle { required property var modelData; width:158*root.uiScale; height:45*root.uiScale; radius:3*root.uiScale; color:"#0D2029"; border.color:Number(modelData.blockers)>0?root.red:(Number(modelData.unresolved)>0?root.amber:root.green)
                        Text { x:7*root.uiScale; y:5*root.uiScale; width:parent.width-14*root.uiScale; elide:Text.ElideRight; text:String(modelData.label||modelData.id).toUpperCase(); color:"white"; font.pixelSize:6*root.uiScale; font.bold:true }
                        Text { x:7*root.uiScale; y:23*root.uiScale; text:String(modelData.resolved)+" / "+String(modelData.total)+"  •  "+String(modelData.completion_percent)+"%"; color:Number(modelData.unresolved)>0?root.amber:root.green; font.pixelSize:6*root.uiScale }
                        Text { anchors { right:parent.right; rightMargin:7*root.uiScale; bottom:parent.bottom; bottomMargin:6*root.uiScale } text:Number(modelData.blockers)>0?String(modelData.blockers)+" BLOCK":""; color:root.red; font.pixelSize:5.5*root.uiScale; font.bold:true }
                        TapHandler { onTapped:{controller.selectOperationalDomain(String(modelData.id));root.workflowPage=1} }
                    }
                }
            }
        }

        Item { id:loadPage; visible:root.workflowPage===2; anchors { left:parent.left; right:parent.right; top:workflowBar.bottom; bottom:statusBar.top; margins:7*root.uiScale }
            Row { anchors.fill:parent; spacing:8*root.uiScale
                Rectangle { width:(parent.width-8*root.uiScale)/2; height:parent.height; color:root.panel; border.color:root.line
                    Text { x:14*root.uiScale; y:14*root.uiScale; text:"LOAD AN EXISTING DRAFT"; color:root.cyan; font.pixelSize:10*root.uiScale; font.bold:true }
                    Text { x:14*root.uiScale; y:40*root.uiScale; width:parent.width-28*root.uiScale; wrapMode:Text.WordWrap; text:"Only EFIS draft V2 files matching this profile and revision are accepted. APPROVED metadata and activation flags are rejected."; color:"#AAB6BC"; font.pixelSize:7*root.uiScale }
                    Rectangle { x:14*root.uiScale; y:106*root.uiScale; width:parent.width-28*root.uiScale; height:38*root.uiScale; radius:3*root.uiScale; color:"#091820"; border.color:root.line
                        TextInput { id:importPath; anchors { fill:parent; margins:8*root.uiScale } color:"white"; font.pixelSize:7*root.uiScale; clip:true; selectByMouse:true; text:String(controller.operationalDraftPath||"") }
                    }
                    Rectangle { x:14*root.uiScale; y:157*root.uiScale; width:parent.width-28*root.uiScale; height:42*root.uiScale; radius:3*root.uiScale; color:"#133C31"; border.color:root.green
                        Text { anchors.centerIn:parent; text:"IMPORT AND STRUCTURE-CHECK"; color:"white"; font.pixelSize:8*root.uiScale; font.bold:true }
                        TapHandler { onTapped:controller.importOperationalDraft(importPath.text) }
                    }
                    Text { x:14*root.uiScale; y:216*root.uiScale; width:parent.width-28*root.uiScale; wrapMode:Text.WordWrap; text:"Current draft file:\n"+String(controller.operationalDraftPath||"NOT CONFIGURED"); color:root.amber; font.pixelSize:6*root.uiScale }
                }
                Rectangle { width:(parent.width-8*root.uiScale)/2; height:parent.height; color:root.panel; border.color:root.red
                    Text { x:14*root.uiScale; y:14*root.uiScale; text:"PREFILL FROM SIMULATION"; color:root.red; font.pixelSize:10*root.uiScale; font.bold:true }
                    Text { x:14*root.uiScale; y:40*root.uiScale; width:parent.width-28*root.uiScale; wrapMode:Text.WordWrap; text:"Copies the complete 447-field analogue scenario into the selected real draft. Every copied field remains SIMULATED and blocks review until replaced with aircraft evidence."; color:"#D4B1B1"; font.pixelSize:7*root.uiScale }
                    Rectangle { x:14*root.uiScale; y:115*root.uiScale; width:parent.width-28*root.uiScale; height:55*root.uiScale; radius:3*root.uiScale; color:"#49151B"; border.color:root.red
                        Text { anchors.centerIn:parent; horizontalAlignment:Text.AlignHCenter; text:"HOLD 1.5s\nLOAD SIMULATION TEMPLATE"; color:"white"; font.pixelSize:8*root.uiScale; font.bold:true }
                        TapHandler { longPressThreshold:1.5; onLongPressed:controller.preloadOperationalSimulation("simulation.asso-v-jolly-912-ul") }
                    }
                    Text { x:14*root.uiScale; y:190*root.uiScale; width:parent.width-28*root.uiScale; wrapMode:Text.WordWrap; text:"This operation overwrites the current staged edits only. It never changes the active aircraft profile."; color:root.amber; font.pixelSize:7*root.uiScale }
                    Rectangle { x:14*root.uiScale; y:257*root.uiScale; width:parent.width-28*root.uiScale; height:40*root.uiScale; radius:3*root.uiScale; color:"#3A211B"; border.color:root.amber
                        Text { anchors.centerIn:parent; text:"HOLD 1.2s • CLEAR CURRENT DRAFT"; color:root.amber; font.pixelSize:7*root.uiScale; font.bold:true }
                        TapHandler { longPressThreshold:1.2; onLongPressed:controller.discardOperationalDraft() }
                    }
                }
            }
        }

        Item { id:validationPage; visible:root.workflowPage===3; anchors { left:parent.left; right:parent.right; top:workflowBar.bottom; bottom:statusBar.top; margins:7*root.uiScale }
            Row { id:validationCards; width:parent.width; height:65*root.uiScale; spacing:7*root.uiScale
                Repeater { model:[{k:"STATE",v:String(root.validationReport.state||"NOT CHECKED"),c:root.validationReport.ready_for_review?root.green:root.red},{k:"ISSUES",v:Number(root.validationReport.issue_count||0),c:root.amber},{k:"SAFETY",v:Number(root.validationReport.safety_blockers||0),c:root.red},{k:"SIM",v:Number(root.validationReport.simulated||0),c:root.red},{k:"EVIDENCE",v:Number(root.validationReport.missing_evidence||0),c:root.amber}]
                    delegate:Rectangle { required property var modelData; width:(validationPage.width-28*root.uiScale)/5; height:65*root.uiScale; radius:3*root.uiScale; color:root.panel; border.color:modelData.c
                        Text { anchors { horizontalCenter:parent.horizontalCenter; top:parent.top; topMargin:7*root.uiScale } text:modelData.k; color:modelData.c; font.pixelSize:6*root.uiScale; font.bold:true }
                        Text { anchors { horizontalCenter:parent.horizontalCenter; bottom:parent.bottom; bottomMargin:9*root.uiScale } width:parent.width-8*root.uiScale; horizontalAlignment:Text.AlignHCenter; elide:Text.ElideRight; text:modelData.v; color:"white"; font.pixelSize:modelData.k==="STATE"?7*root.uiScale:17*root.uiScale; font.bold:true }
                    }
                }
            }
            Rectangle { anchors { left:parent.left; right:parent.right; top:validationCards.bottom; bottom:actions.top; topMargin:7*root.uiScale; bottomMargin:7*root.uiScale } color:root.panel; border.color:root.line
                Text { x:10*root.uiScale; y:7*root.uiScale; text:"ORDERED CORRECTION QUEUE"; color:root.cyan; font.pixelSize:7*root.uiScale; font.bold:true }
                ListView { anchors { fill:parent; margins:8*root.uiScale; topMargin:27*root.uiScale } clip:true; spacing:3*root.uiScale; model:root.validationIssues
                    delegate:Rectangle { required property var modelData; width:ListView.view.width; height:39*root.uiScale; color:"#0D2029"; border.color:String(modelData.code)==="SAFETY_BLOCKER"?root.red:root.line
                        Text { x:8*root.uiScale; y:5*root.uiScale; width:220*root.uiScale; elide:Text.ElideRight; text:String(modelData.code); color:String(modelData.code)==="SIMULATED_VALUE"?root.red:root.amber; font.pixelSize:6*root.uiScale; font.bold:true }
                        Text { x:8*root.uiScale; y:20*root.uiScale; width:270*root.uiScale; elide:Text.ElideRight; text:String(modelData.label); color:"white"; font.pixelSize:6*root.uiScale }
                        Text { x:292*root.uiScale; anchors.verticalCenter:parent.verticalCenter; width:parent.width-300*root.uiScale; elide:Text.ElideRight; text:String(modelData.message); color:"#AAB6BC"; font.pixelSize:6*root.uiScale }
                    }
                }
            }
            Row { id:actions; anchors { left:parent.left; right:parent.right; bottom:parent.bottom } height:42*root.uiScale; spacing:8*root.uiScale
                Rectangle { width:(parent.width-8*root.uiScale)/2; height:parent.height; radius:3*root.uiScale; color:"#143A30"; border.color:root.green
                    Text { anchors.centerIn:parent; text:"RUN COMPLETENESS CHECK"; color:"white"; font.pixelSize:8*root.uiScale; font.bold:true }
                    TapHandler { onTapped:controller.validateOperationalDraft() }
                }
                Rectangle { width:(parent.width-8*root.uiScale)/2; height:parent.height; radius:3*root.uiScale; color:"#15303C"; border.color:root.cyan
                    Text { anchors.centerIn:parent; text:"OPEN FIRST ISSUE"; color:root.cyan; font.pixelSize:8*root.uiScale; font.bold:true }
                    TapHandler { onTapped:{controller.selectNextOperationalIssue();root.workflowPage=1} }
                }
            }
        }

        Rectangle { id:statusBar; anchors { left:parent.left; right:parent.right; bottom:parent.bottom } height:31*root.uiScale; color:"#301015"; border.color:root.red
            Text { x:9*root.uiScale; anchors.verticalCenter:parent.verticalCenter; text:root.summary.simulation_profile?"⛔  SIMULATION DATA • NOT FOR FLIGHT • RELEASE READY = NO":"⛔  DRAFT ONLY • RELEASE READY = NO"; color:root.red; font.pixelSize:7*root.uiScale; font.bold:true }
            Text { anchors { right:parent.right; rightMargin:9*root.uiScale; verticalCenter:parent.verticalCenter } width:420*root.uiScale; horizontalAlignment:Text.AlignRight; elide:Text.ElideLeft; text:String(controller.operationalCatalogStatus||"NOT LOADED"); color:root.amber; font.pixelSize:6.5*root.uiScale; font.bold:true }
        }
    }
}
