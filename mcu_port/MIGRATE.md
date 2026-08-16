# UI 迁移到 APM32F407VG 指南

本目录是从 PC 模拟器复制的**可移植 UI 层**（不依赖 SDL/主机系统），
用于迁移到 MCU。现有工程文件（ui/, sim/）保持不动，其他线程的修改不受影响。

## 目录结构

```
mcu_port/
├── app_ui.c/h        UI 全部界面（开机/主菜单/DAPLink/文件管理/脱机烧录/算法配置）
├── app_algo.c/h      算法配置存储（cfg 解析/生成，纯逻辑，可直接用）
├── app_fs.h          文件系统抽象（MCU: FileX 实现）
├── app_flash.h       烧录抽象（MCU: DAP SWD 实现）
├── app_keys.h        按键抽象（MCU: 3 按键 + 长按）
└── backends/
    ├── app_fs_filex.c     FileX 后端骨架（待补全）
    ├── app_flash_dap.c    DAP 烧录后端骨架（待补全）
    └── app_keys_mcu.c     3 按键 keypad indev 骨架（待接线 GPIO）
```

## 兼容性结论

| 项目 | 结论 |
|---|---|
| 代码依赖 | UI 层纯 LVGL + 抽象接口，无主机依赖 ✓ |
| 非标准函数 | `strcasecmp` 已替换为自包含 `app_stricmp` ✓ |
| 颜色深度 | MCU 已是 RGB565（LV_COLOR_DEPTH 16）与模拟器一致 ✓ |
| 显示 | 复用现有 lvgl_port.c（ST7789 + 部分渲染 240x32x2） ✓ |
| 文件系统 | FileX + W25Q128 已集成，app_fs 后端照着写即可 |
| 多线程 | ThreadX 已有；烧录引擎开独立线程，UI 用 lv_timer 轮询 |
| 字体 | 英文 UI：montserrat 12/14/16/20/24（MCU lv_conf 已启用） |

## 资源评估（APM32F407VG: 512KB Flash / 128KB RAM / 64KB CCM）

| 资源 | 预算 | 说明 |
|---|---|---|
| LVGL 堆 | 建议 32KB（现有 16KB 偏紧） | 弹窗/算法配置/列表同时存在；`LV_MEM_SIZE` |
| 渲染缓冲 | 15KB（240x32x2，现有） | 部分渲染，无需全屏缓冲 |
| UI 任务栈 | 建议 8-16KB | lv_timer_handler + 渲染调用链 |
| Flash | LVGL ~120KB + 字体 ~60KB + UI 代码 ~20KB | 512KB 内可行，需实测 |
| 算法 bin 缓冲 | 4-8KB（目标算法代码大小） | 从 /algo 读入 RAM 再用 SWD 写目标 |

## 移植步骤

1. **加入编译**：把 `mcu_port/app_ui.c`、`app_algo.c` 加入工程（CMake/Keil），
   头文件路径加 `mcu_port/`。
2. **文件系统后端**（app_fs_filex.c）：补全 FileX 调用，挂载现有 W25Q128 卷。
3. **烧录后端**（app_flash_dap.c）：
   - 读默认算法 cfg（app_algo.h）
   - SWD 下载算法 bin 到目标 RAM（ram_addr）
   - 按函数偏移调用 Init/EraseSector/ProgramPage/UnInit（DAP 内存访问走现有 third/DAP）
   - 独立 ThreadX 线程，poll 接口回报进度
4. **按键**（app_keys_mcu.c）：3 GPIO 接线 + 10ms 扫描 + 长按 OK=返回，
   `app_keys_scan()` 挂 ThreadX 定时器。
5. **开机流程**：`app_ui_init()` 由 UI 任务调用；`app_boot_stages()`
   返回真实初始化阶段（挂载/枚举等），进度条反映真实进度。
6. **lv_conf.h**：确认 `LV_MEM_SIZE`（建议 32KB）、12/14/16/20/24 字体已启用
   （当前已启用）。

## 接口对应

| UI 层接口 | PC 模拟器实现 | MCU 实现 |
|---|---|---|
| app_fs_* | sim/sim_fs.c | backends/app_fs_filex.c |
| app_flash_* | sim/sim_flash.c | backends/app_flash_dap.c |
| app_keys_* | sim/sim_key.c | backends/app_keys_mcu.c |
| app_boot_stages | sim/sim_init.c | 真实初始化回调 |

## 注意事项

- 算法 cfg 的 key 名/格式以 `app_algo.c` 为准（与 flm2bin.py 输出一致）。
- 烧录确认弹窗依赖"默认算法"；无默认算法时 UI 会提示进入配置。
- 模拟器与 MCU 的 UI 行为一致（per-screen group、ESC 逐级返回等），
  迁移后交互体验相同。
