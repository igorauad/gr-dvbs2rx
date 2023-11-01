import glob
import json
import logging
import os
import subprocess
import tempfile
import time
from shutil import which
from typing import Any

import click
from gnuradio.dvbs2rx.defs import dvbs2_modcods
from tabulate import tabulate

from .cpu import CpuTop

IMPL_CHOICES = ['dvbs2-rx', 'leandvb']
ROOT_DIR = os.path.dirname(
    os.path.dirname(os.path.dirname(os.path.realpath(__file__))))
DEFAULT_IQ_DIR = os.path.join(ROOT_DIR, 'data', 'iq')


def to_str_list(in_list: list[Any]) -> list[str]:
    """Convert list of objects to list of strings"""
    return list(map(str, in_list))


def get_rec_meta(iq_dir: str) -> dict[str, dict[str, Any]]:
    """Get metadata of all recordings in the IQ directory"""
    meta_files = glob.glob(os.path.join(iq_dir, "*.sigmf-meta"))
    meta_data = {}
    for meta_file in meta_files:
        with open(meta_file, 'r') as fd:
            rec_name = meta_file.replace(iq_dir + '/',
                                         '').replace('.sigmf-meta', '')
            meta_data[rec_name] = json.load(fd)
            meta_data[rec_name]['meta_file'] = meta_file
            meta_data[rec_name]['data_file'] = meta_file.replace(
                'sigmf-meta', 'sigmf-data')
    return meta_data


def run_dvbs2_rx(meta_data, out_temp_file=None, extra_args=[]):
    """Run receiver based on gr-dvbs2rx"""
    global_meta = meta_data['global']
    cmd = to_str_list([
        'dvbs2-rx',
        '--source',
        'file',
        '--in-file',
        meta_data['data_file'],
        '--in-iq-format',
        'u8' if global_meta['core:datatype'] == 'cu8' else 'fc32',
        '--sink',
        'file',
        '--out-file',
        '/dev/null' if out_temp_file is None else out_temp_file.name,
        '--samp-rate',
        global_meta['core:sample_rate'],
        '--sym-rate',
        global_meta['dvbs2:symbol_rate'],
        '--rolloff',
        global_meta['dvbs2:rolloff'],
        '--modcod',
        global_meta['dvbs2:modcod'][0].lower().replace(' ', ''),
        '--frame-size',
        global_meta['dvbs2:fecframe_size'][0],
    ])

    # TODO: VCM is not fully supported yet
    if 'dvbs2:acm_vcm' in global_meta and global_meta['dvbs2:acm_vcm']:
        cmd.append('--pl-acm-vcm')

    if 'dvbs2:pilots' in global_meta:
        cmd.extend([
            '--pilots',
            'on' if global_meta['dvbs2:pilots'] else 'off',
        ])

    cmd.extend(extra_args)
    logging.debug(' '.join(cmd))
    return subprocess.Popen(cmd)


def run_leandvb_rx(meta_data, out_temp_file=None, extra_args=[]):
    """Run receivers based on leandvb"""
    global_meta = meta_data['global']
    in_file = open(meta_data['data_file'], 'rb')

    modcod_bitmask = 0
    for modcod in global_meta['dvbs2:modcod']:
        modcod_str = modcod.lower().replace(' ', '')
        modcod_idx = dvbs2_modcods[modcod_str]
        modcod_bitmask += 2**modcod_idx

    framesize_bitmask = 0
    for frame_size in global_meta['dvbs2:fecframe_size']:
        framesize_bitmask += 1 if frame_size == 'normal' else 2

    cmd = to_str_list([
        'leandvb',
        '--standard',
        'DVB-S2',
        '--nhelpers',
        6,
        '--ldpc-helper',
        which("ldpc_tool"),
        '--sampler',
        'rrc',
        '--rrc-rej',
        30,
        '--u8' if global_meta['core:datatype'] == 'cu8' else '--f32',
        '--modcods',
        modcod_bitmask,
        '-f',
        global_meta['core:sample_rate'],
        '--sr',
        global_meta['dvbs2:symbol_rate'],
        '--roll-off',
        global_meta['dvbs2:rolloff'],
        '--framesizes',
        framesize_bitmask,
    ])
    cmd.extend(extra_args)
    logging.debug(' '.join(cmd) + ' < {}'.format(meta_data['data_file']) +
                  ' > {}'.format('/dev/null' if out_temp_file is
                                 None else out_temp_file.name))

    return subprocess.Popen(
        cmd,
        stdin=in_file,
        stdout=subprocess.DEVNULL if out_temp_file is None else out_temp_file)


def run_and_time(rec_name,
                 meta_data,
                 impl='dvbs2-rx',
                 extra_args=None,
                 cpu=False,
                 plot_cpu=False):
    """Run receiver and measure execution time"""
    out_file = tempfile.NamedTemporaryFile()
    assert impl in IMPL_CHOICES
    if impl == 'dvbs2-rx':
        runner = run_dvbs2_rx
    elif impl == 'leandvb':
        runner = run_leandvb_rx

    extra_args = extra_args.split(',') if extra_args is not None else []

    tic = time.time()
    proc = runner(meta_data, out_file, extra_args)
    if cpu:
        top = CpuTop(proc, impl)
        top.run()
    proc.wait()
    toc = time.time()

    decoded_bytes = os.stat(out_file.name).st_size
    duration = toc - tic

    res = {
        'Implementation': impl,
        'Duration': toc - tic,
        'Decoded Bytes': decoded_bytes,
        'Throughput (Mbps)': 8 * decoded_bytes / duration / 1e6,
    }

    if cpu:
        res['CPU Usage (%)'] = top.get_avg_results_str()

    if plot_cpu:
        if not cpu:
            logging.warning("Cannot plot CPU usage without --cpu flag")
        else:
            top.plot("{}_{}".format(rec_name, impl.replace('-', '_')))

    return res


@click.group()
@click.option('--data-dir',
              default=DEFAULT_IQ_DIR,
              help="Directory containing IQ recordings.")
@click.option('--debug', is_flag=True, help="Enable debugging logs.")
@click.pass_context
def cli(ctx, data_dir, debug):
    """Manage and Test DVB-S2 IQ Recordings"""
    # Ensure that ctx.obj exists and is a dict (in case `cli()` is called by
    # means other than the `if` block below)
    ctx.ensure_object(dict)
    ctx.obj['data_dir'] = data_dir
    logging_level = logging.DEBUG if debug else logging.INFO
    logging.basicConfig(level=logging_level)


@cli.command()
@click.pass_context
def ls(ctx):
    """List SigMF-formatted IQ Recordings"""
    meta_data = get_rec_meta(ctx.obj['data_dir'])
    global_meta = [{
        'name': fname
    } | {
        k.replace('core:', '').replace('dvbs2:', ''):
        (', '.join(v) if isinstance(v, list) else v)
        for k, v in x['global'].items()
        if k not in ['core:sha512', 'core:version', 'file_path']
    } for fname, x in meta_data.items()]
    print(tabulate(global_meta, headers='keys', tablefmt='fancy_grid'))


@cli.command()
@click.argument('rec_name')
@click.option('--impl',
              default='dvbs2-rx',
              type=click.Choice(IMPL_CHOICES),
              help="Demodulator implementation")
@click.option(
    '--args',
    type=str,
    help="Comma-separated list of extra arguments for the demodulator")
@click.option('--cpu',
              is_flag=True,
              help="Measure the CPU utilization of each Rx process thread.")
@click.option('--plot-cpu',
              is_flag=True,
              help="Plot CPU utilization over time.")
@click.pass_context
def rx(ctx, rec_name, impl, args, cpu, plot_cpu):
    """Test Rx processing of IQ recording"""
    repo_meta_data = get_rec_meta(ctx.obj['data_dir'])
    if rec_name not in repo_meta_data:
        logging.error("Recording not found")
        return

    meta_data = repo_meta_data[rec_name]

    try:
        res = run_and_time(rec_name,
                           meta_data,
                           impl=impl,
                           extra_args=args,
                           cpu=cpu,
                           plot_cpu=plot_cpu)
    except KeyboardInterrupt:
        return
    print(tabulate([res], headers='keys', tablefmt='fancy_grid'))


@cli.command()
@click.argument('rec_name')
@click.option('--cpu',
              is_flag=True,
              help="Measure the CPU utilization of each Rx process thread.")
@click.option('--plot-cpu',
              is_flag=True,
              help="Plot CPU utilization over time.")
@click.pass_context
def bench(ctx, rec_name, cpu, plot_cpu):
    """Benchmark Rx performance on IQ recording"""
    repo_meta_data = get_rec_meta(ctx.obj['data_dir'])
    if rec_name not in repo_meta_data:
        logging.error("Recording not found")
        return

    meta_data = repo_meta_data[rec_name]
    try:
        res_dvbs2rx = run_and_time(rec_name,
                                   meta_data,
                                   impl='dvbs2-rx',
                                   cpu=cpu,
                                   plot_cpu=plot_cpu)
        res_leandvb = run_and_time(rec_name,
                                   meta_data,
                                   impl='leandvb',
                                   cpu=cpu,
                                   plot_cpu=plot_cpu)
    except KeyboardInterrupt:
        return
    print(
        tabulate([res_dvbs2rx, res_leandvb],
                 headers='keys',
                 tablefmt='fancy_grid'))


if __name__ == '__main__':
    cli()
