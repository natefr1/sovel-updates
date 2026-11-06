"""Exercise archive, transfer and productivity helpers with real temporary files."""

import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile
import uuid


PACKAGING = Path(__file__).resolve().parents[1] / "packaging"


def run(program, *arguments, env=None, request=b"", succeeds=True):
    proc = subprocess.run([sys.executable, str(PACKAGING / program), *map(str, arguments)],
                          input=request, capture_output=True, env=env, timeout=45)
    if succeeds:
        assert proc.returncode == 0, proc.stderr.decode()
        return [json.loads(line) for line in proc.stdout.splitlines() if line.startswith(b"{")]
    assert proc.returncode != 0, proc.stdout.decode()
    return proc.stderr.decode()


def main():
    with tempfile.TemporaryDirectory(prefix="sovel-update09-") as temporary:
        base = Path(temporary)
        source = base / "fixtures"
        source.mkdir()
        data = (b"Real Sovel archive and transfer fixture.\n" * 4096)
        original = source / "project-notes.txt"
        original.write_bytes(data)
        for extension in ("zip", "7z", "tar", "tar.gz", "tar.xz", "gz"):
            target = base / f"project-notes.{extension}"
            run("sovel-archive", "create", target, "6", original)
            preview = run("sovel-archive", "preview", target)[-1]["entries"]
            assert len(preview) == 1 and preview[0]["name"].startswith("project-notes")
            run("sovel-archive", "test", target)
            destination = base / ("extract-" + extension)
            destination.mkdir()
            run("sovel-archive", "extract", target, destination, "here")
            assert next(destination.iterdir()).read_bytes() == data
            assert "already exists" in run("sovel-archive", "extract", target, destination, "here", succeeds=False)
            assert len(list(destination.iterdir())) == 1
        hostile = base / "traversal.tar"
        with tarfile.open(hostile, "w") as archive:
            entry = tarfile.TarInfo("../outside.txt")
            entry.size = 3
            archive.addfile(entry, io.BytesIO(b"bad"))
        assert "Unsafe archive path" in run("sovel-archive", "extract", hostile, base / "extract-zip", "here", succeeds=False)
        assert not (base / "outside.txt").exists()
        hostile = base / "symlink.tar"
        with tarfile.open(hostile, "w") as archive:
            entry = tarfile.TarInfo("link")
            entry.type = tarfile.SYMTYPE
            entry.linkname = "/tmp"
            archive.addfile(entry)
        assert "link or special file" in run("sovel-archive", "extract", hostile, base / "extract-zip", "here", succeeds=False)
        print("ZIP, 7Z, TAR, TAR.GZ, TAR.XZ and GZ round-trip, conflict and malicious archive checks passed.", flush=True)

        env = dict(os.environ, XDG_DATA_HOME=str(base / "data"), XDG_STATE_HOME=str(base / "state"),
                   HOME=str(base))
        destination = base / "transferred"
        destination.mkdir()
        queue = base / "state/Sovel/transfers"
        queue.mkdir(parents=True)

        def job(move=False, control=None):
            identifier = uuid.uuid4().hex
            path = queue / (identifier + ".json")
            path.write_text(json.dumps({"id": identifier, "state": "queued", "sources": [str(original)],
                                        "destination": str(destination), "move": move}))
            if control:
                path.with_suffix(".control").write_text(control)
            run("sovel-transfer", identifier, env=env)
            return json.loads(path.read_text())

        assert job(control="cancel")["state"] == "cancelled"
        assert not list(destination.iterdir())
        assert job()["state"] == "complete"
        assert original.read_bytes() == (destination / original.name).read_bytes() == data
        assert job()["state"] == "failed"
        (destination / original.name).unlink()
        assert job(move=True)["state"] == "complete"
        assert not original.exists() and (destination / original.name).read_bytes() == data
        print("Detached-job format verified for copy, move, cancel and no-overwrite conflict.", flush=True)

        def workspace(action, **arguments):
            return run("sovel-jared-workspace", action, env=env, request=json.dumps(arguments).encode())[-1]

        project = workspace("project-add", path=str(destination), name="Sovel")["workspace"]["projects"][0]
        note = workspace("note-save", title="Installer", body="Test archive extraction", project=project["id"])
        assert workspace("note-read", id=note["id"])["record"]["body"] == "Test archive extraction"
        workspace("note-save", id=note["id"], title="Installer checklist", body="Test copy and move",
                  folder="Release", project=project["id"])
        assert workspace("note-read", id=note["id"])["record"]["folder"] == "Release"
        task = workspace("task-add", title="Check updates", project=project["id"])["workspace"]["tasks"][0]
        assert workspace("task-toggle", id=task["id"])["workspace"]["tasks"][0]["done"]
        assert workspace("find", query="project notes txt")["matches"][0]["path"] == str(destination / original.name)
        assert workspace("project-open", name="sovel")["path"] == str(destination)
        recent = workspace("find", query="recent", project=project["id"])["matches"]
        assert recent and all(Path(item["path"]).is_relative_to(destination) for item in recent)
        checklist = destination / "checklist.md"
        checklist.write_text("- [ ] Check display restore\n- [x] Finished work\nTODO: Verify USB eject\n", encoding="utf-8")
        preview = workspace("task-extract", path=str(checklist), project=project["id"])
        assert preview["tasks"] == ["Check display restore", "Verify USB eject"]
        assert not any(row["title"] == "Verify USB eject" for row in workspace("list")["workspace"]["tasks"])
        arguments = dict(project=project["id"], tasks=preview["tasks"])
        workspace("task-add-list", **arguments)
        workspace("task-add-list", **arguments)
        assert sum(row["title"] == "Verify USB eject" for row in workspace("list")["workspace"]["tasks"]) == 1
        summary = workspace("project-summary", project=project["id"])
        assert summary["project"] == project["id"]
        assert any(item["path"] == str(checklist) for item in summary["files"])
        task = next(row for row in workspace("list")["workspace"]["tasks"] if row["title"] == "Verify USB eject")
        workspace("task-set", id=task["id"], done=True)
        workspace("task-set", id=task["id"], done=True)
        assert next(row for row in workspace("list")["workspace"]["tasks"] if row["id"] == task["id"])["done"]
        assert workspace("open", path=str(destination / original.name))["path"] == str(destination / original.name)
        workspace("research-save", url="https://example.com/", title="Reference", body="Saved excerpt",
                  project=project["id"])
        assert workspace("list")["workspace"]["research"][0]["title"] == "Reference"
        assert workspace("web-search", query="Sovel desktop")["url"] == "https://duckduckgo.com/?q=Sovel%20desktop"
        failure = run("sovel-jared-workspace", "article", env=env,
                      request=b'{"url":"https://127.0.0.1/"}', succeeds=False)
        assert "public internet hosts" in failure
        print("Real note create/read/edit, projects, task toggling, scoped search and saved research passed.", flush=True)


if __name__ == "__main__":
    main()
