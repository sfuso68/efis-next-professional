#pragma once

// Cattura della pagina motore, su richiesta e a programma in marcia.
//
// Il nodo EMS gira su eglfs, senza compositore, quindi grim e gli altri
// strumenti Wayland qui non esistono. La cattura comandata da EFIS_CAPTURE
// risolve solo il caso in cui si avvia il programma apposta: per fotografare
// cio' che il pannello sta mostrando davvero bisognerebbe fermare il servizio,
// e a quel punto si fotografa un altro avvio, non quello in corso.
//
// SIGUSR1 toglie di mezzo il problema. Il servizio gira come `pi`, quindi
// `kill -USR1` non richiede root: la cattura dal display reale, che e' la sola
// verifica valida per una modifica di interfaccia, torna disponibile da una
// sessione remota qualsiasi senza toccare cio' che sta girando.

#include <QString>

class QQuickWindow;

namespace efis::ems {

// Salva la finestra in `path`. Vero se il file e' stato scritto davvero.
[[nodiscard]] bool capture_window(QQuickWindow* window, const QString& path);

// Installa il gestore di SIGUSR1: a ogni segnale salva una schermata in
// `directory`, con un nome che porta l'istante. Restituisce falso dove il
// segnale non esiste, cioe' su Windows, e dove non si e' potuto installare;
// in nessuno dei due casi e' un errore fatale, il nodo continua a volare.
bool install_capture_signal(QQuickWindow* window, const QString& directory);

}  // namespace efis::ems
