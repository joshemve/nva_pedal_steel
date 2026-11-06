"""Run Codex headless image-generation jobs in parallel and harvest the PNGs.

Usage: python run_codex_images.py <jobs.json> <out_dir>
jobs.json: [{"name": "v1", "prompt": "...", "images": ["ref/a.png", ...]}]
Each job's images land in <out_dir>/<name>_<n>.png; logs in <out_dir>/logs/.
"""
import json, os, shutil, subprocess, sys, glob, time
from concurrent.futures import ThreadPoolExecutor

CODEX = max(glob.glob(os.path.expandvars(r"%LOCALAPPDATA%\OpenAI\Codex\bin\*\codex.exe")), key=os.path.getmtime)
GEN = os.path.expanduser(r"~\.codex\generated_images")
HERE = os.path.dirname(os.path.abspath(__file__))

def run(job, out):
    os.makedirs(os.path.join(out, "logs"), exist_ok=True)
    log = os.path.join(out, "logs", job["name"] + ".jsonl")
    cmd = [CODEX, "exec", "--json", "--skip-git-repo-check", "-s", "read-only", "-C", HERE]
    for im in job.get("images", []):
        cmd += ["-i", os.path.join(HERE, im)]
    prompt = job["prompt"] + "\n\nUse your image generation tool only. Do not write code or run commands. Generate the image(s) and reply with one line describing each."
    t0 = time.time()
    with open(log, "w", encoding="utf-8") as f:
        subprocess.run(cmd + ["-"], input=prompt, text=True, encoding="utf-8", stdout=f, stderr=subprocess.STDOUT)
    thread = None
    for line in open(log, encoding="utf-8", errors="replace"):
        try:
            ev = json.loads(line)
        except Exception:
            continue
        if ev.get("type") == "thread.started":
            thread = ev.get("thread_id")
    got = []
    if thread and os.path.isdir(os.path.join(GEN, thread)):
        for k, p in enumerate(sorted(glob.glob(os.path.join(GEN, thread, "*.png")), key=os.path.getmtime)):
            dst = os.path.join(out, f"{job['name']}_{k+1}.png")
            shutil.copy2(p, dst); got.append(dst)
    return job["name"], thread, round(time.time() - t0), got

if __name__ == "__main__":
    jobs = json.load(open(sys.argv[1], encoding="utf-8"))
    out = os.path.abspath(sys.argv[2])
    with ThreadPoolExecutor(len(jobs)) as ex:
        for name, thread, secs, got in ex.map(lambda j: run(j, out), jobs):
            print(f"{name}: thread={thread} {secs}s images={len(got)} {got}", flush=True)
