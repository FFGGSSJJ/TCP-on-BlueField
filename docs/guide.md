# Guide for DOCA Develop



### Issues

This part records issues that I encoutered during the development.\

------



### 1.DOCA Version Problem

The original local doca environment on the BlueField Card is really out of date. And I found the card cannot access the internet, in which case I cannot update the `doca-runtime`, `doca-tools` and `doca-sdk`.

If you encounter any problem in the doca library file during the compilation, update the environment by `apt-install`.

Refer to this official doc for details: 

- https://docs.nvidia.com/doca/sdk/installation-guide-for-linux/index.html

------



### 2.DPDK Problem

The original system only downloaded the `dpdk` release (located at `~/dpdk-20.11`) and did not build and install it at all.... It took me one afternoon to finally figure out that there is no `dpdk lib` installed in this system at all as I thought the environment has been settled down before I use it.

A short pieces of problems I encountered:

```shell
drivers:
	common/mvep:	missing dependency, "libmusdk"
	net/af_xdp:	missing dependency, "libbpf"
	net/ipn3ke:	missing dependency, "libfdt"
	net/mlx4:	missing dependency, "mlx4"
	net/mvneta:	missing dependency, "libmusdk"
	net/mvpp2:	missing dependency, "libmusdk"
	net/nfb:	missing dependency, "libnfb"
	net/szedata2:	missing dependency, "libsze2"
	raw/ifpga:	missing dependency, "libfdt"
	raw/ioat:	only supported on x86
	crypto/aesni_gcm:	missing dependency, "libIPSec_MB"
	crypto/aesni_mb:	missing dependency, "libIPSec_MB"
	crypto/armv8:	missing dependency, "libAArch64crypto"
	crypto/kasumi:	missing dependency, "libIPSec_MB"
	crypto/mvsam:	missing dependency, "libmusdk"
	crypto/snow3g:	missing dependency, "libIPSec_MB"
	crypto/zuc:	missing dependency, "libIPSec_MB"
	compress/isal:	missing dependency, "libisal"
	event/dlb:	only supported on x86_64 Linux
	event/dlb2:	only supported on x86_64 Linux
```



If you see any problem related to `dpdk`, I am not sure what is happening. I should have set everything down.

Refer to these blogs for details:

- https://support.mellanox.com/s/article/Configuring-DPDK-and-Running-testpmd-on-BlueField-2
- https://medium.com/codex/nvidia-mellanox-bluefield-2-smartnic-dpdk-rig-for-dive-part-ii-change-mode-of-operation-a994f0f0e543

------



### 3.RTE_FORCE_INTRINSICS

When I tried to compile certain sample doca codes provided, I encountered one error as shown:

```shell
/usr/local/include/rte_****.h:9:4: error: #error Platform must be built with RTE_FORCE_INTRINSICS
    9 | #  error Platform must be built with RTE_FORCE_INTRINSICS
      |    ^~~~~
...
```

No idea why this occurs. Accroding to this [blog](https://medium.com/codex/nvidia-mellanox-bluefield-2-smartnic-dpdk-rig-for-dive-part-ii-change-mode-of-operation-a994f0f0e543), I simply modify the header file and comment that sentence out.

------



### 4.undefined symbol of dpdk

The error occurs when I tried to execute `doca_flow_drop`:

```shell
./doca_flow_drop: symbol lookup error: /opt/mellanox/doca/lib/aarch64-linux-gnu/libdoca_flow.so.1: undefined symbol: rte_mtr_meter_policy_add, version EXPERIMENTAL
```

I checked the `dpdk` lib and found out that `rte_mtr_meter_policy_add` is a function in `rte_mtr.h`. 

In current environment (i mean in my current environment), I can find it under `/opt/mellanox/dpdk/include/dpdk`. However, I ***cannot find*** the pkg link file of it in `/opt/mellanox/dpdk/lib`, as you can check in the pkgconfig file `/opt/mellanox/dpdk/lib/aarch64-linux-gnu/pkgconfig/libdpdk.pc` show here:

```yaml
prefix=/opt/mellanox/dpdk
libdir=${prefix}/lib/aarch64-linux-gnu
includedir=${prefix}/include/dpdk

Name: DPDK
Description: The Data Plane Development Kit (DPDK).
Note that CFLAGS might contain an -march flag higher than typical baseline.
This is required for a number of static inline functions in the public headers.
Version: 20.11.5.2.2
Requires: libdpdk-libs, libbsd
Requires.private: libmlx5, libibverbs, libelf, libpcap, jansson
Libs.private: -Wl,--whole-archive -L${libdir} -l:librte_bus_auxiliary.a -l:librte_bus_pci.a -l:librte_bus_vdev.a -l:librte_common_mlx5.a -l:librte_mempool_bucket.a -l:librte_mempool_ring.a -l:librte_mempool_stack.a -l:librte_net_af_packet.a -l:librte_net_mlx5.a -l:librte_net_vhost.a -l:librte_net_virtio.a -l:librte_compress_mlx5.a -l:librte_regex_mlx5.a -l:librte_vdpa_mlx5.a -l:librte_gpu_cuda.a -l:librte_node.a -l:librte_graph.a -l:librte_bpf.a -l:librte_flow_classify.a -l:librte_pipeline.a -l:librte_table.a -l:librte_port.a -l:librte_fib.a -l:librte_ipsec.a -l:librte_vhost.a -l:librte_stack.a -l:librte_security.a -l:librte_sched.a -l:librte_reorder.a -l:librte_rib.a -l:librte_regexdev.a -l:librte_rawdev.a -l:librte_pdump.a -l:librte_power.a -l:librte_member.a -l:librte_lpm.a -l:librte_latencystats.a -l:librte_kni.a -l:librte_jobstats.a -l:librte_gso.a -l:librte_gro.a -l:librte_gpudev.a -l:librte_eventdev.a -l:librte_efd.a -l:librte_distributor.a -l:librte_cryptodev.a -l:librte_compressdev.a -l:librte_cfgfile.a -l:librte_bitratestats.a -l:librte_bbdev.a -l:librte_acl.a -l:librte_timer.a -l:librte_metrics.a -l:librte_cmdline.a -l:librte_pci.a -l:librte_ethdev.a -l:librte_meter.a -l:librte_ip_frag.a -l:librte_net.a -l:librte_mbuf.a -l:librte_mempool.a -l:librte_hash.a -l:librte_rcu.a -l:librte_ring.a -l:librte_eal.a -l:librte_telemetry.a -l:librte_kvargs.a -Wl,--no-whole-archive -Wl,--export-dynamic -lmtcr_ul
Cflags: -I${includedir}
```



2022-10-31: I checked over the internet and docs, and finally decided to reinstall the `dpdk` lib with a stable version: `stable-20.11.1`.

2022-11-02: failed to install `dpdk-stable-20.11.1` due to another environment problem and I decided to turn to `docker` container environment for development.... no idea what should I do when it comes to deployment



