#!/usr/bin/env python3
"""Offline, replay-only Vosk adapter for EFIS Next development.

The adapter emits recognition candidates. It never decides an intent and has
no transport, radio, Pixhawk or actuator output. VoiceCommandGate remains the
only component allowed to accept a candidate as a pilot-interface intent.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import sys
import time
import uuid
import wave
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Iterable


CANDIDATE_SCHEMA = "efis-next.voice-recognition-candidate.v1"
GRAMMAR_SCHEMA = "efis-next.vosk-decoder-grammar.v1"
SUPPORTED_LANGUAGES = ("it-IT", "en-GB")


class AdapterError(RuntimeError):
    """Expected validation or recognizer failure."""


def canonical_json(value: Any) -> bytes:
    return json.dumps(
        value, ensure_ascii=False, sort_keys=True, separators=(",", ":")
    ).encode("utf-8")


def load_grammar(path: Path, language: str) -> tuple[list[str], str]:
    try:
        document = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise AdapterError(f"cannot load grammar: {exc}") from exc

    if document.get("schema") != GRAMMAR_SCHEMA:
        raise AdapterError("unsupported grammar schema")
    if document.get("authority") != "DECODER_HINT_ONLY":
        raise AdapterError("grammar must be marked DECODER_HINT_ONLY")
    languages = document.get("languages")
    if not isinstance(languages, dict) or language not in languages:
        raise AdapterError(f"grammar does not contain language {language}")
    phrases = languages[language]
    if (
        not isinstance(phrases, list)
        or not phrases
        or any(not isinstance(item, str) or not item.strip() for item in phrases)
    ):
        raise AdapterError("grammar phrase list is empty or invalid")
    normalized = [item.strip().lower() for item in phrases]
    if len(normalized) != len(set(normalized)):
        raise AdapterError("grammar contains duplicate phrases")
    return normalized, hashlib.sha256(canonical_json(document)).hexdigest()


def validate_wave(path: Path) -> int:
    try:
        with wave.open(str(path), "rb") as audio:
            channels = audio.getnchannels()
            sample_width = audio.getsampwidth()
            sample_rate = audio.getframerate()
            compression = audio.getcomptype()
            frames = audio.getnframes()
    except (OSError, EOFError, wave.Error) as exc:
        raise AdapterError(f"cannot read WAV: {exc}") from exc
    if channels != 1 or sample_width != 2 or sample_rate != 16000 or compression != "NONE":
        raise AdapterError("WAV must be PCM16, mono, uncompressed, 16000 Hz")
    if frames <= 0:
        raise AdapterError("WAV contains no audio frames")
    return frames


def confidence_summary(result: dict[str, Any]) -> tuple[float, float, int]:
    words = result.get("result")
    if not isinstance(words, list) or not words:
        return 0.0, 0.0, 0
    values: list[float] = []
    for word in words:
        confidence = word.get("conf") if isinstance(word, dict) else None
        if (
            isinstance(confidence, bool)
            or not isinstance(confidence, (int, float))
            or not math.isfinite(float(confidence))
            or confidence < 0.0
            or confidence > 1.0
        ):
            raise AdapterError("recognizer returned invalid word confidence")
        values.append(float(confidence))
    return min(values), sum(values) / len(values), len(values)


def candidate_from_result(
    result: dict[str, Any],
    *,
    session_id: str,
    sequence: int,
    language: str,
    grammar_sha256: str,
    model_id: str,
    push_to_talk_active: bool,
) -> dict[str, Any] | None:
    transcript = result.get("text")
    if not isinstance(transcript, str):
        raise AdapterError("recognizer result has no string text field")
    transcript = " ".join(transcript.lower().split())
    if not transcript:
        return None
    minimum, mean, count = confidence_summary(result)
    return {
        "schema": CANDIDATE_SCHEMA,
        "session_id": session_id,
        "sequence": sequence,
        "emitted_utc": datetime.now(timezone.utc).isoformat(timespec="milliseconds"),
        "monotonic_ns": time.monotonic_ns(),
        "source_node": "BLACKBOX-VOICE-SIM",
        "recognizer": "VOSK",
        "recognizer_healthy": True,
        "model_id": model_id,
        "language": language,
        "grammar_sha256": grammar_sha256,
        "transcript": transcript,
        "confidence": minimum,
        "confidence_mean": mean,
        "confidence_word_count": count,
        "push_to_talk_active": push_to_talk_active,
        "simulated": True,
        "candidate_only": True,
        "intent": "NONE",
        "accepted": False,
        "transmission_or_actuation_allowed": False,
    }


def _decoded_results(recognizer: Any, audio: wave.Wave_read, chunk_frames: int) -> Iterable[dict[str, Any]]:
    while True:
        data = audio.readframes(chunk_frames)
        if not data:
            break
        if recognizer.AcceptWaveform(data):
            try:
                yield json.loads(recognizer.Result())
            except json.JSONDecodeError as exc:
                raise AdapterError("recognizer returned malformed result JSON") from exc
    try:
        yield json.loads(recognizer.FinalResult())
    except json.JSONDecodeError as exc:
        raise AdapterError("recognizer returned malformed final JSON") from exc


def recognize_replay(
    *,
    model_path: Path,
    wav_path: Path,
    language: str,
    phrases: list[str],
    grammar_sha256: str,
    push_to_talk_active: bool,
    session_id: str,
    chunk_frames: int = 4000,
    vosk_api: Any | None = None,
) -> list[dict[str, Any]]:
    if chunk_frames <= 0:
        raise AdapterError("chunk_frames must be positive")
    validate_wave(wav_path)
    if not model_path.is_dir():
        raise AdapterError("Vosk model directory does not exist")
    if vosk_api is None:
        try:
            import vosk as vosk_api  # type: ignore[no-redef]
        except ImportError as exc:
            raise AdapterError(
                "Vosk Python module is not installed; see VOICE_CORE_BLACKBOX.md"
            ) from exc

    try:
        model = vosk_api.Model(str(model_path))
        recognizer = vosk_api.KaldiRecognizer(
            model, 16000, json.dumps(phrases, ensure_ascii=False)
        )
        recognizer.SetWords(True)
        model_id = model_path.name
        candidates: list[dict[str, Any]] = []
        sequence = 0
        with wave.open(str(wav_path), "rb") as audio:
            for result in _decoded_results(recognizer, audio, chunk_frames):
                candidate = candidate_from_result(
                    result,
                    session_id=session_id,
                    sequence=sequence + 1,
                    language=language,
                    grammar_sha256=grammar_sha256,
                    model_id=model_id,
                    push_to_talk_active=push_to_talk_active,
                )
                if candidate is not None:
                    sequence += 1
                    candidates.append(candidate)
        return candidates
    except AdapterError:
        raise
    except Exception as exc:
        raise AdapterError(f"Vosk replay failed: {exc}") from exc


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--model", type=Path, required=True, help="local Vosk model directory")
    parser.add_argument("--wav", type=Path, required=True, help="PCM16 mono 16 kHz replay file")
    parser.add_argument(
        "--grammar",
        type=Path,
        default=Path("config/voice/vosk-grammar-v1.json"),
        help="restricted decoder grammar",
    )
    parser.add_argument("--language", choices=SUPPORTED_LANGUAGES, required=True)
    parser.add_argument("--simulate", action="store_true", help="required safety acknowledgement")
    parser.add_argument("--ptt-active", action="store_true", help="required simulated PTT gate")
    parser.add_argument("--session-id", help="optional replay session identifier")
    return parser.parse_args(argv)


def validate_safety_flags(args: argparse.Namespace) -> None:
    if not args.simulate:
        raise AdapterError("--simulate is mandatory; live mode is not implemented")
    if not args.ptt_active:
        raise AdapterError("--ptt-active is mandatory; continuous listening is forbidden")


def main(argv: list[str] | None = None) -> int:
    try:
        args = parse_args(argv)
        validate_safety_flags(args)
        phrases, grammar_sha256 = load_grammar(args.grammar, args.language)
        candidates = recognize_replay(
            model_path=args.model,
            wav_path=args.wav,
            language=args.language,
            phrases=phrases,
            grammar_sha256=grammar_sha256,
            push_to_talk_active=True,
            session_id=args.session_id or str(uuid.uuid4()),
        )
        for candidate in candidates:
            print(json.dumps(candidate, ensure_ascii=False, sort_keys=True))
        return 0
    except AdapterError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
