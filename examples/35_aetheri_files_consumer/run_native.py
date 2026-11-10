"""Run original Files native assertions with an explicit candidate SDK binding.

SDK selection/provenance and peer header paths are recorded adaptations.
Optional capture observations add independent read-only native expectations.
The original source export and its reviewed SDK pin are never written.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--original", type=Path, required=True)
    parser.add_argument("--sdk-input", type=Path, required=True)
    parser.add_argument("--expected-sdk-revision", required=True)
    parser.add_argument("--linkage", choices=("shared", "static"), default="shared")
    parser.add_argument("--mode", choices=("normal", "fault"), default="normal")
    parser.add_argument("--atspi", type=Path)
    parser.add_argument("--observe-captures", action="store_true",
                        help="Require independent native text/focus expectations before four original captures")
    for name in ("server", "app", "font", "terminal", "editor", "mono", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    if args.observe_captures and (args.mode != "normal" or not args.atspi):
        parser.error("--observe-captures requires normal mode and --atspi")
    here = Path(__file__).resolve().parent
    original = args.original.resolve(strict=True)
    for manifest in ("source-inputs.json", "native-inputs.json"):
        subprocess.run([sys.executable, str(here / "check_source.py"),
                        str(original), str(here / manifest)], check=True)
    sdk = json.loads(args.sdk_input.read_text())
    if (not re.fullmatch(r"[0-9a-f]{40}", args.expected_sdk_revision)
            or sdk["revision"] != args.expected_sdk_revision
            or sdk["version"] not in ("0.1.23", "0.1.33") or sdk["atspi_compiled"] is not True):
        raise SystemExit("Wrong candidate SDK declaration")
    library = Path(sdk["library"]).resolve(strict=True)
    if library.name != "libnexaui.so." + sdk["version"] or sha256(library) != sdk["sha256"]:
        raise SystemExit("Changed candidate SDK library")
    dbus = Path(sdk["dbus_sdk"]).resolve(strict=True)
    for item in sdk["dbus_headers"]:
        path = dbus / item["path"]
        if dbus not in path.resolve(strict=True).parents or sha256(path) != item["sha256"]:
            raise SystemExit("Changed peer D-Bus header")
    if not sdk["dbus_headers"]:
        raise SystemExit("Missing peer D-Bus headers")
    static_archive = None
    static_metadata = None
    if args.linkage == "static":
        binding = sdk.get("static_link", {})
        if not all(name in binding for name in ("archive", "sha256", "app", "app_sha256")):
            raise SystemExit("Static native checks require the exact archive and application binding")
        static_archive = Path(binding["archive"]).resolve(strict=True)
        if (static_archive.name != "libnexaui.a" or sha256(static_archive) != binding["sha256"]
                or Path(binding["app"]).resolve(strict=True) != args.app.resolve(strict=True)
                or sha256(args.app) != binding["app_sha256"]):
            raise SystemExit("Changed static SDK archive or application")
        static_metadata = dict(sdk, sha256=binding["sha256"])
    args.output.mkdir(parents=True, exist_ok=True)
    suffix = "_fault" if args.mode == "fault" else ""
    driver = original / ("tools/test_files_view" + suffix + ".py")
    source = driver.read_text()
    changes = []

    def replace_once(before, after):
        nonlocal source
        if source.count(before) != 1:
            raise SystemExit("Original native driver does not match the reviewed adaptation")
        source = source.replace(before, after)
        changes.append({"before": before, "after": after})

    if args.mode == "normal":
        replace_once("sdk=Path(json.loads((ROOT/'spec/nexaui-inputs.json').read_text())['development_builds']['release'])/'dbus-sdk'",
                     "sdk=_candidate_dbus")
        replace_once('''library=loaded.pop();candidate=library.parents[2];metadata=json.loads((candidate/"build.json").read_text())
        pin=json.loads((ROOT/"spec/nexaui-inputs.json").read_text())
        assert str(candidate) in pin["development_builds"].values() and library.name=="libnexaui.so."+pin["version"]
        assert metadata["revision"]==pin["revision"] and metadata["version"]==pin["version"] and metadata["atspi_compiled"]
        assert digest(library)==metadata["artifacts"][library.relative_to(candidate).as_posix()]''',
                     "library=loaded.pop();metadata=_candidate_metadata\n"
                     "        assert library==_candidate_library and digest(library)==metadata['sha256']")
        if args.linkage == "static":
            replace_once(";assert len(loaded)==1", ";assert not loaded")
            replace_once("library=loaded.pop();metadata=_candidate_metadata\n"
                         "        assert library==_candidate_library and digest(library)==metadata['sha256']",
                         "library=_candidate_static_archive;metadata=_candidate_static_metadata\n"
                         "        assert digest(library)==metadata['sha256']\n"
                         "        assert app_bin==_candidate_bound_app and digest(app_bin)==metadata['static_link']['app_sha256']")
        replace_once('''result["font"]={"path":str(font),"sha256":digest(font)}''',
                     '''result["sdk"]["linkage"]=_candidate_linkage
        result["font"]={"path":str(font),"sha256":digest(font)}''')
    elif args.atspi:
        raise SystemExit("The original fault driver has no AT-SPI mode")
    if args.observe_captures:
        from capture_observation import prepare
        source, observations = prepare(source)
        changes.extend(observations)
    adapted = args.output / ("candidate-test-files-view" + suffix + ".py")
    adapted.write_text(source)
    record = {"original_driver": str(driver), "original_sha256": sha256(driver),
              "adapted_sha256": sha256(adapted), "changes": changes,
              "sdk_input": sdk, "sdk_input_sha256": sha256(args.sdk_input),
              "linkage": args.linkage,
              "observe_captures": args.observe_captures,
              "scope": "Explicit candidate experiment; original reviewed pin preserved; all original behavior/assertions/deadlines retained, with optional read-only capture expectations"}
    (args.output / ("driver-adaptation" + suffix + ".json")).write_text(json.dumps(record, indent=2) + "\n")
    sys.argv = [str(driver)] + [str(getattr(args, name).resolve(strict=True)) for name in
                               ("server", "app", "font", "terminal", "editor", "mono", "output")]
    if args.atspi:
        sys.argv += ["--atspi", str(args.atspi.resolve(strict=True))]
    namespace = {"__name__": "__main__", "__file__": str(driver),
                 "_candidate_metadata": sdk, "_candidate_library": library, "_candidate_dbus": dbus,
                 "_candidate_static_archive": static_archive, "_candidate_static_metadata": static_metadata,
                 "_candidate_bound_app": args.app.resolve(strict=True), "_candidate_linkage": args.linkage,
                 "_capture_observer_source": here / "ime_atspi.c"}
    exec(compile(source, str(adapted), "exec"), namespace)


if __name__ == "__main__":
    main()
