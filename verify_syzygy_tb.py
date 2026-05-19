import chess
import chess.syzygy
import csv
import os

TB_PATH = os.getenv("TBPROBE_TESTS_SYZYGY_PATH", "tb")
CSV_PATH = os.getenv("TBPROBE_TESTS_SYZYGY_CSV", "tests.csv")

with chess.syzygy.open_tablebase(TB_PATH) as tb:
    with open(CSV_PATH, newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)

        for row in reader:
            fen = row["fen"]
            expected_wdl = int(row["wdl"])
            expected_dtz = int(row["dtz"])

            board = chess.Board(fen)

            try:
                actual_wdl = tb.probe_wdl(board)
                actual_dtz = tb.probe_dtz(board)
            except Exception as e:
                raise RuntimeError(
                    f'Probing failed for FEN "{fen}": {e}'
                ) from e

            assert expected_wdl == actual_wdl, (
                f"WDL mismatch for {fen}: "
                f"expected {expected_wdl}, got {actual_wdl}"
            )

            assert expected_dtz == actual_dtz, (
                f"DTZ mismatch for {fen}: "
                f"expected {expected_dtz}, got {actual_dtz}"
            )

print("All tests passed (it should)")
