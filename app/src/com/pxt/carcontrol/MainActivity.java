package com.pxt.carcontrol;

import android.Manifest;
import android.app.Activity;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothManager;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.text.method.ScrollingMovementMethod;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.ArrayAdapter;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

import java.text.SimpleDateFormat;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Locale;
import java.util.Set;

/**
 * 收集车控制台 V3.1。
 *
 * 与 Fusion_OpenCar 固件的对应关系（app_config.h / app_chassis.c）：
 *   底盘档位 gear 0~6  →  目标转速 400 + (gear-3)*100 r/min（100~700）
 *   叶片档位 aux  0~6  →  占空比   100 - (6-aux)*5  %（70~100）
 *   T/U 叶片升/降档，X/Y 底盘升/降档，越界回中间档 3；
 *   A~H 八方向，S 停止。全部单字节。
 *
 * 叶片显示与车端不一致的两个来源，本版都已处理：
 *   1) 指令节流：固件 200 ms 去抖，APP 以 260 ms 间隔排队发送，避免“APP 加一档、车端没加”；
 *   2) 车端回传：支持 {D<叶片占空比>:<底盘档位>}$ 帧，收到后以车端数值刷新界面
 *      （需固件端增加该帧，见 README 的补丁说明；未收到时显示“本地估计”）。
 */
public class MainActivity extends Activity {

    private static final int REQ_PERM = 1001;
    private static final int REQ_ENABLE = 1002;

    /* 底盘 7 档：400 ± 100 r/min */
    private static final int CH_COUNT = 7;
    private static final int CH_CENTER = 3;
    private static final int CH_BASE_RPM = 400;
    private static final int CH_STEP_RPM = 100;

    /* 叶片 7 档：70% ~ 100%，每档 5%，上电 100%（gear 6），中间档 85% */
    private static final int AUX_COUNT = 7;
    private static final int AUX_CENTER = 3;
    private static final int AUX_BOOT = 6;
    private static final int AUX_TOP = 100;
    private static final int AUX_STEP = 5;

    private static final long CMD_GAP_MS = 260;

    private final Handler ui = new Handler(Looper.getMainLooper());
    private final BtLink link = new BtLink();
    private final List<BluetoothDevice> devices = new ArrayList<>();

    private BluetoothAdapter adapter;
    private ArrayAdapter<String> deviceAdapter;
    private Spinner spinner;

    private TextView statusView, dirView, connBtn, telemetry, logView;
    private TextView chValue, chMeta, auxValue, auxMeta, auxSync;
    private GearBar chBar, auxBar;
    private JoystickPad pad;

    private int chGear = CH_CENTER;
    private int auxGear = AUX_BOOT;
    private boolean connected = false;
    private boolean syncedFromCar = false;

    /* 档位指令队列：间隔 >= 260 ms */
    private final ArrayDeque<Runnable> cmdQueue = new ArrayDeque<>();
    private final Handler cmdHandler = new Handler(Looper.getMainLooper());
    private long lastCmdAt = 0;
    private boolean draining = false;
    private final Runnable drainTask = new Runnable() {
        @Override
        public void run() {
            draining = false;
            drainQueue();
        }
    };

    // ------------------------------------------------------------- 生命周期

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        getWindow().setStatusBarColor(0xFF0B3D91);

        BluetoothManager bm = (BluetoothManager) getSystemService(Context.BLUETOOTH_SERVICE);
        adapter = bm == null ? null : bm.getAdapter();

        setContentView(buildUi());

        link.setListener(new BtLink.Listener() {
            @Override
            public void onState(final String text) {
                ui.post(new Runnable() {
                    @Override
                    public void run() {
                        status(text);
                        if (text.startsWith("已连接")) {
                            connected = true;
                            connBtn.setText("断开");
                            pad.setEnabledState(true);
                        } else if (text.startsWith("连接失败") || text.startsWith("连接已断开")) {
                            connected = false;
                            connBtn.setText("连接");
                            pad.setEnabledState(false);
                        }
                    }
                });
            }

            @Override
            public void onLine(final String text) {
                ui.post(new Runnable() {
                    @Override
                    public void run() {
                        onFrame(text);
                    }
                });
            }
        });

        pad.setEnabledState(false);
        updateChassisUi();
        updateAuxUi();
        ensureBluetooth();
    }

    @Override
    protected void onPause() {
        super.onPause();
        if (connected) link.send("S");
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        link.close();
    }

    // ------------------------------------------------------------- 界面

    private int dp(float v) {
        return Ui.dp(this, v);
    }

    private View buildUi() {
        ScrollView scroll = new ScrollView(this);
        scroll.setBackgroundColor(Ui.BG);
        scroll.setFillViewport(true);

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(0, 0, 0, dp(18));
        scroll.addView(root, new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        root.addView(buildHeader(), new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        root.addView(buildDeviceCard(), cardLp());
        root.addView(buildPadCard(), cardLp());
        root.addView(buildChassisCard(), cardLp());
        root.addView(buildAuxCard(), cardLp());
        root.addView(buildStopButton(), stopLp());

        telemetry = Ui.label(this, "轮速 --　电量 --　航向 --", 11.5f, Ui.MUTED, false);
        telemetry.setGravity(Gravity.CENTER);
        telemetry.setPadding(dp(8), dp(10), dp(8), dp(2));
        root.addView(telemetry);

        root.addView(buildLogCard(), cardLp());
        return scroll;
    }

    private LinearLayout.LayoutParams cardLp() {
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        lp.setMargins(dp(14), dp(6), dp(14), dp(6));
        return lp;
    }

    private LinearLayout.LayoutParams stopLp() {
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, dp(54));
        lp.setMargins(dp(14), dp(12), dp(14), dp(6));
        return lp;
    }

    private View buildHeader() {
        LinearLayout h = new LinearLayout(this);
        h.setOrientation(LinearLayout.VERTICAL);
        h.setPadding(dp(20), dp(20), dp(20), dp(20));

        GradientDrawable bg = Ui.vertical(this, 0xFF0B3D91, 0xFF1565C0, 0);
        bg.setCornerRadii(new float[]{0, 0, 0, 0, dp(22), dp(22), dp(22), dp(22)});
        h.setBackground(bg);

        h.addView(Ui.label(this, "收集车控制台", 22, Color.WHITE, true));

        TextView sub = Ui.label(this, "BLUETOOTH · SPP · HC-02", 10, 0x99FFFFFF, false);
        sub.setLetterSpacing(0.12f);
        sub.setPadding(0, dp(4), 0, dp(10));
        h.addView(sub);

        statusView = Ui.label(this, "未连接", 12.5f, 0xCCFFFFFF, true);
        h.addView(statusView);
        return h;
    }

    private LinearLayout card() {
        LinearLayout c = new LinearLayout(this);
        c.setOrientation(LinearLayout.VERTICAL);
        c.setPadding(dp(16), dp(14), dp(16), dp(14));
        Ui.card(c, this);
        return c;
    }

    private TextView sectionTitle(String text) {
        TextView t = Ui.label(this, text, 13, Ui.TEXT, true);
        t.setLetterSpacing(0.02f);
        return t;
    }

    private View buildDeviceCard() {
        LinearLayout c = card();
        c.addView(sectionTitle("设备连接"));

        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setGravity(Gravity.CENTER_VERTICAL);
        row.setPadding(0, dp(10), 0, 0);

        spinner = new Spinner(this, Spinner.MODE_DROPDOWN);
        deviceAdapter = new ArrayAdapter<>(this, android.R.layout.simple_spinner_item,
                new ArrayList<String>());
        deviceAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        spinner.setAdapter(deviceAdapter);
        spinner.setBackground(Ui.roundStroke(this, 0xFFF4F7FB, Ui.LINE, 12));
        spinner.setPadding(dp(8), 0, dp(8), 0);
        row.addView(spinner, new LinearLayout.LayoutParams(0, dp(44), 1f));

        TextView refresh = ghostButton("刷新");
        LinearLayout.LayoutParams rp = new LinearLayout.LayoutParams(dp(62), dp(44));
        rp.leftMargin = dp(8);
        row.addView(refresh, rp);
        refresh.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                loadDevices();
            }
        });

        connBtn = primaryButton("连接");
        LinearLayout.LayoutParams cp = new LinearLayout.LayoutParams(dp(74), dp(44));
        cp.leftMargin = dp(8);
        row.addView(connBtn, cp);
        connBtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                toggleConnect();
            }
        });

        c.addView(row);
        return c;
    }

    private View buildPadCard() {
        LinearLayout c = card();
        c.addView(sectionTitle("方向控制"));

        pad = new JoystickPad(this);
        LinearLayout.LayoutParams pl = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, dp(286));
        pl.topMargin = dp(6);
        c.addView(pad, pl);
        pad.setOnDirectionListener(new JoystickPad.OnDirectionListener() {
            @Override
            public void onDirection(int code) {
                if (!connected) {
                    log("未连接，方向指令未发送");
                    return;
                }
                if (code == 0) {
                    sendDirection("S", "停止");
                } else {
                    char letter = (char) ('A' + code - 1);
                    sendDirection(String.valueOf(letter), nameOf(code));
                }
                pad.performHapticFeedback(android.view.HapticFeedbackConstants.VIRTUAL_KEY);
            }
        });

        dirView = Ui.label(this, "当前指令：停止", 13, Ui.MUTED, false);
        dirView.setGravity(Gravity.CENTER);
        dirView.setPadding(0, dp(6), 0, 0);
        c.addView(dirView);
        return c;
    }

    private View buildChassisCard() {
        LinearLayout c = card();

        LinearLayout top = new LinearLayout(this);
        top.setOrientation(LinearLayout.HORIZONTAL);
        top.setGravity(Gravity.CENTER_VERTICAL);
        top.addView(sectionTitle("底盘速度"), new LinearLayout.LayoutParams(0,
                ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
        chMeta = Ui.label(this, "", 11.5f, Ui.MUTED, false);
        top.addView(chMeta);
        c.addView(top);

        chBar = new GearBar(this);
        LinearLayout.LayoutParams bl = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, dp(10));
        bl.topMargin = dp(12);
        c.addView(chBar, bl);

        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setGravity(Gravity.CENTER_VERTICAL);
        row.setPadding(0, dp(12), 0, 0);

        TextView minus = stepButton("−");
        row.addView(minus, new LinearLayout.LayoutParams(dp(58), dp(52)));
        holdRepeat(minus, new Runnable() {
            @Override
            public void run() {
                step("Y", false, -1);
            }
        });

        LinearLayout mid = new LinearLayout(this);
        mid.setOrientation(LinearLayout.VERTICAL);
        mid.setGravity(Gravity.CENTER);
        chValue = Ui.label(this, "", 28, Ui.TEXT, true);
        chValue.setGravity(Gravity.CENTER);
        mid.addView(chValue);
        TextView unit = Ui.label(this, "r/min", 10.5f, Ui.MUTED, false);
        unit.setGravity(Gravity.CENTER);
        mid.addView(unit);
        row.addView(mid, new LinearLayout.LayoutParams(0,
                ViewGroup.LayoutParams.WRAP_CONTENT, 1f));

        TextView plus = stepButton("＋");
        row.addView(plus, new LinearLayout.LayoutParams(dp(58), dp(52)));
        holdRepeat(plus, new Runnable() {
            @Override
            public void run() {
                step("X", false, 1);
            }
        });

        c.addView(row);
        return c;
    }

    private View buildAuxCard() {
        LinearLayout c = card();

        LinearLayout top = new LinearLayout(this);
        top.setOrientation(LinearLayout.HORIZONTAL);
        top.setGravity(Gravity.CENTER_VERTICAL);
        top.addView(sectionTitle("叶片转速"), new LinearLayout.LayoutParams(0,
                ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
        auxMeta = Ui.label(this, "", 11.5f, Ui.MUTED, false);
        top.addView(auxMeta);
        c.addView(top);

        auxBar = new GearBar(this);
        LinearLayout.LayoutParams bl = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, dp(10));
        bl.topMargin = dp(12);
        c.addView(auxBar, bl);

        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setGravity(Gravity.CENTER_VERTICAL);
        row.setPadding(0, dp(12), 0, 0);

        TextView minus = stepButton("−");
        row.addView(minus, new LinearLayout.LayoutParams(dp(58), dp(52)));
        holdRepeat(minus, new Runnable() {
            @Override
            public void run() {
                step("U", true, -1);
            }
        });

        LinearLayout mid = new LinearLayout(this);
        mid.setOrientation(LinearLayout.VERTICAL);
        mid.setGravity(Gravity.CENTER);
        auxValue = Ui.label(this, "", 28, Ui.TEXT, true);
        auxValue.setGravity(Gravity.CENTER);
        mid.addView(auxValue);
        TextView unit = Ui.label(this, "占空比 %", 10.5f, Ui.MUTED, false);
        unit.setGravity(Gravity.CENTER);
        mid.addView(unit);
        row.addView(mid, new LinearLayout.LayoutParams(0,
                ViewGroup.LayoutParams.WRAP_CONTENT, 1f));

        TextView plus = stepButton("＋");
        row.addView(plus, new LinearLayout.LayoutParams(dp(58), dp(52)));
        holdRepeat(plus, new Runnable() {
            @Override
            public void run() {
                step("T", true, 1);
            }
        });

        c.addView(row);

        auxSync = Ui.label(this, "本地估计：车端回传后自动校准", 10.5f, Ui.MUTED, false);
        auxSync.setPadding(0, dp(10), 0, 0);
        c.addView(auxSync);
        return c;
    }

    private View buildStopButton() {
        TextView stop = Ui.label(this, "停  止", 17, Color.WHITE, true);
        stop.setGravity(Gravity.CENTER);
        stop.setBackground(Ui.pressable(this, Ui.STOP, 0xFFB71C1C, Ui.STOP, 16));
        stop.setElevation(dp(2));
        stop.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                sendDirection("S", "停止");
                dirView.setText("当前指令：停止");
            }
        });
        return stop;
    }

    private View buildLogCard() {
        LinearLayout c = card();
        c.addView(sectionTitle("运行日志"));

        logView = Ui.label(this, "", 9.5f, Ui.MUTED, false);
        logView.setTypeface(Typeface.MONOSPACE);
        logView.setBackground(Ui.round(this, 0xFFF7F9FC, 12));
        logView.setPadding(dp(10), dp(8), dp(10), dp(8));
        logView.setMovementMethod(new ScrollingMovementMethod());
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, dp(96));
        lp.topMargin = dp(10);
        c.addView(logView, lp);
        return c;
    }

    private TextView ghostButton(String text) {
        TextView b = Ui.label(this, text, 12.5f, Ui.PRIMARY, true);
        b.setGravity(Gravity.CENTER);
        b.setBackground(Ui.pressable(this, 0xFFF4F7FB, 0xFFE3EBF6, Ui.LINE, 12));
        return b;
    }

    private TextView primaryButton(String text) {
        TextView b = Ui.label(this, text, 13, Color.WHITE, true);
        b.setGravity(Gravity.CENTER);
        b.setBackground(Ui.pressable(this, Ui.PRIMARY, Ui.PRIMARY_D, Ui.PRIMARY, 12));
        b.setElevation(dp(1));
        return b;
    }

    private TextView stepButton(String text) {
        TextView b = Ui.label(this, text, 20, Ui.PRIMARY, true);
        b.setGravity(Gravity.CENTER);
        b.setBackground(Ui.pressable(this, 0xFFF4F7FB, 0xFFE3EBF6, 0xFFD8E2EF, 14));
        return b;
    }

    /** 按住连发：首次立即执行，之后每 300 ms 一次（配合 260 ms 队列间隔）。 */
    private void holdRepeat(final TextView v, final Runnable action) {
        final Handler h = new Handler(Looper.getMainLooper());
        final Runnable repeat = new Runnable() {
            @Override
            public void run() {
                action.run();
                h.postDelayed(this, 300);
            }
        };
        v.setOnTouchListener(new View.OnTouchListener() {
            @Override
            public boolean onTouch(View view, MotionEvent e) {
                switch (e.getActionMasked()) {
                    case MotionEvent.ACTION_DOWN:
                        h.removeCallbacks(repeat);
                        action.run();
                        h.postDelayed(repeat, 300);
                        return true;
                    case MotionEvent.ACTION_UP:
                    case MotionEvent.ACTION_CANCEL:
                        h.removeCallbacks(repeat);
                        return true;
                }
                return false;
            }
        });
    }

    // ------------------------------------------------------------- 指令

    /** 档位指令入队：发送成功后才更新显示，两道指令间隔 >= 260 ms。 */
    private void step(final String cmd, final boolean aux, final int dir) {
        if (!connected) {
            log("未连接，忽略指令 " + cmd);
            return;
        }
        enqueue(new Runnable() {
            @Override
            public void run() {
                link.send(cmd);
                if (aux) {
                    auxGear = wrap(auxGear + dir, AUX_COUNT, AUX_CENTER);
                    updateAuxUi();
                } else {
                    chGear = wrap(chGear + dir, CH_COUNT, CH_CENTER);
                    updateChassisUi();
                }
                log("→ " + cmd + (aux ? " 叶片" : " 底盘") + (dir > 0 ? "升档" : "降档"));
            }
        });
    }

    private void enqueue(Runnable r) {
        cmdQueue.add(r);
        drainQueue();
    }

    private void drainQueue() {
        if (draining || cmdQueue.isEmpty()) return;
        long now = SystemClock.uptimeMillis();
        long wait = lastCmdAt + CMD_GAP_MS - now;
        if (wait > 0) {
            draining = true;
            cmdHandler.postDelayed(drainTask, wait);
            return;
        }
        Runnable r = cmdQueue.poll();
        lastCmdAt = SystemClock.uptimeMillis();
        if (r != null) r.run();
        if (!cmdQueue.isEmpty()) {
            draining = true;
            cmdHandler.postDelayed(drainTask, CMD_GAP_MS);
        }
    }

    /** 越界回中间档，与固件 aux_gear_change()/chassis_gear_change() 一致。 */
    private int wrap(int gear, int count, int center) {
        if (gear < 0 || gear >= count) return center;
        return gear;
    }

    private int chassisRpm() {
        return CH_BASE_RPM + (chGear - CH_CENTER) * CH_STEP_RPM;
    }

    private int auxDuty() {
        return AUX_TOP - ((AUX_COUNT - 1) - auxGear) * AUX_STEP;
    }

    private int gearFromDuty(int duty) {
        int g = Math.round((duty - (AUX_TOP - (AUX_COUNT - 1) * AUX_STEP)) / (float) AUX_STEP);
        if (g < 0) g = 0;
        if (g > AUX_COUNT - 1) g = AUX_COUNT - 1;
        return g;
    }

    private void updateChassisUi() {
        chValue.setText(String.valueOf(chassisRpm()));
        chMeta.setText("第 " + (chGear + 1) + " / " + CH_COUNT + " 档");
        chBar.set(CH_COUNT, chGear);
    }

    private void updateAuxUi() {
        auxValue.setText(String.valueOf(auxDuty()));
        auxMeta.setText("第 " + (auxGear + 1) + " / " + AUX_COUNT + " 档");
        auxBar.set(AUX_COUNT, auxGear);
    }

    private String nameOf(int code) {
        String[] n = {"", "前进", "右前", "右转", "右后", "后退", "左后", "左转", "左前"};
        return n[code];
    }

    private void sendDirection(String cmd, String label) {
        if (!connected) {
            log("未连接，忽略指令 " + cmd);
            return;
        }
        link.send(cmd);
        log("→ " + cmd + "  " + label);
        if (cmd.equals("S")) {
            dirView.setText("当前指令：停止");
        } else if (cmd.charAt(0) >= 'A' && cmd.charAt(0) <= 'H') {
            dirView.setText("当前指令：" + label + "（" + cmd + "）");
        }
    }

    private void status(String s) {
        statusView.setText(s);
        log(s);
    }

    /** 车端回传：{A..}$ 轮速/电量/航向，{B..}$ 姿态，{C..}$ 参数，{D..}$ 档位。 */
    private void onFrame(String frame) {
        try {
            String body = frame;
            if (body.endsWith("}")) body = body.substring(0, body.length() - 1);

            if (body.startsWith("{A") && body.length() > 6) {
                String[] p = body.substring(2).split(":");
                if (p.length >= 4) {
                    telemetry.setText("轮速 A " + p[0].trim() + " · B " + p[1].trim()
                            + " r/min　电量 " + p[2].trim() + "%　航向 " + p[3].trim() + "°");
                }
            } else if (body.startsWith("{B")) {
                String[] p = body.substring(2).split(":");
                if (p.length >= 3) {
                    log("姿态：pitch " + p[0].trim() + " roll " + p[1].trim()
                            + " yaw " + p[2].trim());
                }
            } else if (body.startsWith("{C")) {
                String[] p = body.substring(2).split(":");
                if (p.length >= 2) {
                    log("参数 C：" + p[0].trim() + " / " + p[1].trim());
                }
            } else if (body.startsWith("{D")) {
                String[] p = body.substring(2).split(":");
                if (p.length >= 2) {
                    int duty = Integer.parseInt(p[0].trim());
                    int ch = Integer.parseInt(p[1].trim());
                    auxGear = gearFromDuty(duty);
                    chGear = Math.max(0, Math.min(CH_COUNT - 1, ch));
                    updateAuxUi();
                    updateChassisUi();
                    if (!syncedFromCar) {
                        syncedFromCar = true;
                        auxSync.setText("已与车端同步（以车端占空比为准）");
                        auxSync.setTextColor(0xFF16A34A);
                    }
                }
            }
        } catch (Exception ignored) {
        }
    }

    private void log(String s) {
        String t = new SimpleDateFormat("HH:mm:ss", Locale.CHINA).format(new Date());
        String old = logView.getText().toString();
        String all = (old.isEmpty() ? "" : old + "\n") + t + "  " + s;
        String[] arr = all.split("\n");
        if (arr.length > 80) {
            StringBuilder sb = new StringBuilder();
            for (int i = arr.length - 80; i < arr.length; i++) sb.append(arr[i]).append('\n');
            all = sb.toString().trim();
        }
        logView.setText(all);
        logView.post(new Runnable() {
            @Override
            public void run() {
                if (logView.getParent() instanceof ScrollView) {
                    ((ScrollView) logView.getParent()).fullScroll(View.FOCUS_DOWN);
                }
            }
        });
    }

    // ------------------------------------------------------------- 蓝牙

    private void ensureBluetooth() {
        if (adapter == null) {
            status("此手机没有蓝牙功能");
            return;
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S
                && checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT)
                != PackageManager.PERMISSION_GRANTED) {
            requestPermissions(new String[]{Manifest.permission.BLUETOOTH_CONNECT}, REQ_PERM);
            return;
        }
        if (!adapter.isEnabled()) {
            startActivityForResult(new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE), REQ_ENABLE);
            return;
        }
        loadDevices();
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == REQ_PERM) {
            if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                ensureBluetooth();
            } else {
                status("需要蓝牙连接权限才能读取已配对设备");
            }
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == REQ_ENABLE && resultCode == RESULT_OK) loadDevices();
    }

    private void loadDevices() {
        if (adapter == null) return;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S
                && checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT)
                != PackageManager.PERMISSION_GRANTED) {
            return;
        }
        devices.clear();
        deviceAdapter.clear();
        try {
            Set<BluetoothDevice> bonded = adapter.getBondedDevices();
            for (BluetoothDevice d : bonded) devices.add(d);
        } catch (SecurityException e) {
            status("没有蓝牙权限：" + e.getMessage());
            return;
        }
        if (devices.isEmpty()) {
            deviceAdapter.add("未发现已配对设备");
            status("没有已配对设备，请先在系统设置中配对 HC-02");
        } else {
            int pick = 0;
            for (int i = 0; i < devices.size(); i++) {
                String name = devices.get(i).getName() == null ? "未知设备" : devices.get(i).getName();
                deviceAdapter.add(name + "  " + devices.get(i).getAddress());
                String up = name.toUpperCase(Locale.US);
                if (up.contains("HC") || up.contains("JDY") || up.contains("BT")) pick = i;
            }
            spinner.setSelection(pick);
            status("找到 " + devices.size() + " 个已配对设备");
        }
        deviceAdapter.notifyDataSetChanged();
    }

    private void toggleConnect() {
        if (connected) {
            link.close();
            connected = false;
            connBtn.setText("连接");
            pad.setEnabledState(false);
            status("已断开连接");
            return;
        }
        if (devices.isEmpty()) {
            Toast.makeText(this, "请先配对蓝牙模块", Toast.LENGTH_SHORT).show();
            return;
        }
        int pos = spinner.getSelectedItemPosition();
        if (pos < 0 || pos >= devices.size()) return;
        status("正在连接 " + devices.get(pos).getAddress() + " …");
        link.connect(devices.get(pos));
    }
}
