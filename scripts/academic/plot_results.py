"""Render shareable study figures with Matplotlib (not screenshots of UI charts)."""
import argparse
import csv
import json
from pathlib import Path

def plot(study, destination):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    # Matplotlib 3.11+ shapes/reorders complex scripts natively with libraqm.
    # Pre-reordering on those versions would reverse Persian a second time.
    if tuple(int(x) for x in matplotlib.__version__.split('.')[:2]) >= (3,11):
        fa=lambda text:text
        plt.rcParams['text.language']='fa'
    else:
        import arabic_reshaper
        from bidi.algorithm import get_display
        fa=lambda text:get_display(arabic_reshaper.reshape(text))
    from matplotlib import font_manager
    font=Path('C:/Windows/Fonts/tahoma.ttf')
    if font.exists(): font_manager.fontManager.addfont(str(font))
    plt.rcParams.update({"font.family":"Tahoma" if font.exists() else "DejaVu Sans","font.size":10,"axes.spines.top":False,
                         "axes.spines.right":False,"figure.dpi":130,"savefig.bbox":"tight"})
    destination.mkdir(parents=True,exist_ok=True)
    data=json.loads((Path("docs/academic")/"results.json").read_text())
    rows=[r for r in data["results"] if r["scan_ports"]==20]
    def save(fig,name):
        fig.tight_layout(); fig.savefig(destination/(name+".png")); fig.savefig(destination/(name+".svg")); plt.close(fig)
    with (study/"default/DoS-SYN-stu/traffic_per_second.csv").open(newline="") as f:
        seconds=list(csv.DictReader(f))
    base=int(seconds[0]["timestamp"].split('.')[0]); values={int(r["timestamp"].split('.')[0])-base:int(r["packets"]) for r in seconds}
    x=list(range(max(values)+1)); y=[values.get(t,0) for t in x]
    fig,ax=plt.subplots(figsize=(9,3.5)); ax.plot(x,y,color="#174b83",lw=1)
    ax.axvline(1665163502.374618-base,color="#bf4b30",ls="--",label=fa("اولین SYN حمله در مشاهده مستقل"))
    ax.set(xlabel=fa("زمان کپچر از اولین ثانیه کامل (s)"),ylabel=fa("همه بسته‌ها در هر ثانیه"),title=fa("فایل SYN؛ ترافیک دقیق یک‌ثانیه‌ای با لحاظ ثانیه‌های صفر"));ax.legend(fontsize=8);save(fig,"syn-timeline")
    with Path("docs/academic/sensitivity.csv").open(newline="") as f: sensitivity=list(csv.DictReader(f))
    fig,(a,b)=plt.subplots(1,2,figsize=(9,3.6))
    labels=[r["scan_ports"] for r in sensitivity];positions=range(len(labels))
    a.plot(list(positions),[float(r["precision"]) for r in sensitivity],"o-",label=fa("Precision همه دسته‌ها"))
    a.plot(list(positions),[float(r["recall"]) for r in sensitivity],"s-",label=fa("Recall همه دسته‌ها"))
    a.plot(list(positions),[float(r["scan_recall"]) for r in sensitivity],"^-",label=fa("Recall فقط اسکن"))
    a.set_xticks(list(positions),labels);a.set(xlabel=fa("آستانه تعداد پورت متمایز مقصد"),ylabel=fa("نسبت"),ylim=(-.03,1.05));a.legend(fontsize=8)
    b.bar(labels,[int(r["scan_FP"]) for r in sensitivity],color="#567da8");b.set(xlabel=fa("آستانه پورت"),ylabel=fa("گروه هشدار اسکن خارج از مرجع"))
    fig.suptitle(fa("حساسیت آستانه با فایل و مرجع ثابت؛ پنجره ۱۰ ثانیه"));save(fig,"threshold-sensitivity")
    fig,(a,b)=plt.subplots(1,2,figsize=(10,4))
    names=[r["capture"].replace("-stu","") for r in rows];pos=list(range(len(rows)))
    a.barh(pos,[r["packets_per_second"]/1000 for r in rows],color="#174b83");a.set_yticks(pos,names);a.invert_yaxis();a.set_xlabel(fa("هزار بسته در هر ثانیه پردازش"))
    b.barh(pos,[r["peak_memory_bytes"]/1048576 for r in rows],color="#2c8d6e");b.set_yticks(pos,names);b.invert_yaxis();b.set_xlabel(fa("بیشینه حافظه فرآیند (MiB)"))
    fig.suptitle(fa("ویندوز Release با خروجی CSV؛ یک اجرا برای هر فایل، بدون بازه اطمینان"));save(fig,"performance")
    with (study/"default/DoS-slowloris-stu/http_log.csv").open(newline="") as f:
        slow=[r for r in csv.DictReader(f) if r["slow"]=="true"]
    from decimal import Decimal
    ages=[float(Decimal(r["last"])-Decimal(r["first"])) for r in slow]
    fig,ax=plt.subplots(figsize=(8,3.5));ax.hist(ages,bins=20,color="#174b83",edgecolor="white")
    ax.set(xlabel=fa("سن header ناتمام در ثبت نهایی (s)"),ylabel=fa("تعداد رکورد HTTP"),title=fa(f"شواهد Slowloris؛ {len(slow)} زمینه واجدشرایط؛ سن حسگر معادل اثر سرویس نیست"))
    save(fig,"slowloris-ages")

if __name__=="__main__":
    p=argparse.ArgumentParser(description=__doc__);p.add_argument("--study",type=Path,required=True)
    p.add_argument("--destination",type=Path,default=Path("docs/academic/figures"))
    a=p.parse_args();plot(a.study,a.destination)
