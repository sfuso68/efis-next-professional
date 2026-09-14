#!/usr/bin/env python3

import json
import tempfile
import unittest
import wave
from argparse import Namespace
from pathlib import Path

import vosk_replay_adapter as adapter


class FakeRecognizer:
    def __init__(self, _model, sample_rate, grammar):
        self.sample_rate = sample_rate
        self.grammar = json.loads(grammar)
        self.words_enabled = False
        self.returned = False

    def SetWords(self, enabled):
        self.words_enabled = enabled

    def AcceptWaveform(self, _data):
        if not self.returned:
            self.returned = True
            return True
        return False

    def Result(self):
        return json.dumps({
            "text": "confermo",
            "result": [{"word": "confermo", "conf": 0.91}],
        })

    def FinalResult(self):
        return json.dumps({"text": ""})


class FakeVosk:
    class Model:
        def __init__(self, path):
            self.path = path

    KaldiRecognizer = FakeRecognizer


class VoskReplayAdapterTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.model = self.root / "model-it-test"
        self.model.mkdir()
        self.wav = self.root / "sample.wav"
        with wave.open(str(self.wav), "wb") as audio:
            audio.setnchannels(1)
            audio.setsampwidth(2)
            audio.setframerate(16000)
            audio.writeframes(b"\0\0" * 8000)

    def tearDown(self):
        self.temporary.cleanup()

    def test_replay_emits_candidate_without_authority(self):
        candidates = adapter.recognize_replay(
            model_path=self.model,
            wav_path=self.wav,
            language="it-IT",
            phrases=["confermo", "fatto"],
            grammar_sha256="a" * 64,
            push_to_talk_active=True,
            session_id="session-test",
            vosk_api=FakeVosk,
        )
        self.assertEqual(len(candidates), 1)
        candidate = candidates[0]
        self.assertEqual(candidate["schema"], adapter.CANDIDATE_SCHEMA)
        self.assertEqual(candidate["transcript"], "confermo")
        self.assertEqual(candidate["confidence"], 0.91)
        self.assertTrue(candidate["candidate_only"])
        self.assertFalse(candidate["accepted"])
        self.assertEqual(candidate["intent"], "NONE")
        self.assertFalse(candidate["transmission_or_actuation_allowed"])

    def test_confidence_is_conservative_minimum(self):
        minimum, mean, count = adapter.confidence_summary({
            "result": [{"conf": 0.96}, {"conf": 0.73}, {"conf": 0.88}]
        })
        self.assertEqual(minimum, 0.73)
        self.assertAlmostEqual(mean, (0.96 + 0.73 + 0.88) / 3)
        self.assertEqual(count, 3)
        self.assertEqual(adapter.confidence_summary({"text": "confirm"}), (0.0, 0.0, 0))

    def test_invalid_confidence_is_rejected(self):
        for value in (-0.1, 1.1, float("nan"), "0.9", True):
            with self.subTest(value=value), self.assertRaises(adapter.AdapterError):
                adapter.confidence_summary({"result": [{"conf": value}]})

    def test_wave_format_is_fail_closed(self):
        bad = self.root / "stereo.wav"
        with wave.open(str(bad), "wb") as audio:
            audio.setnchannels(2)
            audio.setsampwidth(2)
            audio.setframerate(16000)
            audio.writeframes(b"\0\0\0\0" * 20)
        with self.assertRaises(adapter.AdapterError):
            adapter.validate_wave(bad)

    def test_grammar_is_decoder_hint_and_language_scoped(self):
        grammar = self.root / "grammar.json"
        grammar.write_text(json.dumps({
            "schema": adapter.GRAMMAR_SCHEMA,
            "authority": "DECODER_HINT_ONLY",
            "languages": {"it-IT": ["confermo"], "en-GB": ["confirm"]},
        }), encoding="utf-8")
        phrases, digest = adapter.load_grammar(grammar, "en-GB")
        self.assertEqual(phrases, ["confirm"])
        self.assertEqual(len(digest), 64)

    def test_safety_flags_refuse_live_or_open_microphone_mode(self):
        with self.assertRaises(adapter.AdapterError):
            adapter.validate_safety_flags(Namespace(simulate=False, ptt_active=True))
        with self.assertRaises(adapter.AdapterError):
            adapter.validate_safety_flags(Namespace(simulate=True, ptt_active=False))

    def test_candidate_requires_word_confidence_for_nonzero_score(self):
        candidate = adapter.candidate_from_result(
            {"text": "confirm"},
            session_id="s",
            sequence=1,
            language="en-GB",
            grammar_sha256="b" * 64,
            model_id="model",
            push_to_talk_active=True,
        )
        self.assertIsNotNone(candidate)
        self.assertEqual(candidate["confidence"], 0.0)
        self.assertEqual(candidate["confidence_word_count"], 0)


if __name__ == "__main__":
    unittest.main()
