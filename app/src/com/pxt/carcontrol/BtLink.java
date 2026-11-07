package com.pxt.carcontrol;

import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothSocket;

import java.io.InputStream;
import java.io.OutputStream;
import java.util.UUID;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * 经典蓝牙 SPP（RFCOMM）连接管理：连接、收发、断开。
 * 车端回传帧以 '$' 结尾，这里按 '$' 或换行切分后交给界面。
 */
public class BtLink {

    public interface Listener {
        void onState(String text);

        void onLine(String text);
    }

    private static final UUID SPP_UUID =
            UUID.fromString("00001101-0000-1000-8000-00805F9B34FB");

    private final ExecutorService io = Executors.newSingleThreadExecutor();
    private BluetoothSocket socket;
    private OutputStream out;
    private InputStream in;
    private volatile boolean open;
    private Listener listener;

    public void setListener(Listener l) {
        this.listener = l;
    }

    public boolean isOpen() {
        return open;
    }

    public void connect(final BluetoothDevice device) {
        io.execute(new Runnable() {
            @Override
            public void run() {
                try {
                    close();
                    socket = device.createRfcommSocketToServiceRecord(SPP_UUID);
                    socket.connect();
                    out = socket.getOutputStream();
                    in = socket.getInputStream();
                    open = true;
                    state("已连接：" + safeName(device));
                    startReader();
                } catch (Exception e) {
                    open = false;
                    state("连接失败：" + e.getMessage());
                }
            }
        });
    }

    private String safeName(BluetoothDevice d) {
        try {
            String n = d.getName();
            return n == null ? d.getAddress() : n;
        } catch (Exception e) {
            return d.getAddress();
        }
    }

    private void startReader() {
        new Thread(new Runnable() {
            @Override
            public void run() {
                byte[] buf = new byte[256];
                StringBuilder frame = new StringBuilder();
                while (open) {
                    try {
                        int n = in.read(buf);
                        if (n < 0) break;
                        for (int i = 0; i < n; i++) {
                            char c = (char) (buf[i] & 0xFF);
                            if (c == '$' || c == '\n' || c == '\r') {
                                if (frame.length() > 0) {
                                    line(frame.toString());
                                    frame.setLength(0);
                                }
                            } else if (c >= 0x20 || c > 0x7F) {
                                frame.append(c);
                            }
                        }
                        if (frame.length() > 160) {
                            line(frame.toString());
                            frame.setLength(0);
                        }
                    } catch (Exception e) {
                        break;
                    }
                }
                if (open) {
                    open = false;
                    state("连接已断开");
                }
            }
        }, "bt-reader").start();
    }

    /** 发送单字节指令（车端按单字节解析，不能带换行）。 */
    public void send(final String s) {
        io.execute(new Runnable() {
            @Override
            public void run() {
                if (!open || out == null) {
                    state("发送失败：未连接");
                    return;
                }
                try {
                    out.write(s.getBytes("US-ASCII"));
                    out.flush();
                } catch (Exception e) {
                    state("发送失败：" + e.getMessage());
                }
            }
        });
    }

    public void close() {
        open = false;
        try {
            if (socket != null) socket.close();
        } catch (Exception ignored) {
        }
        socket = null;
        out = null;
        in = null;
    }

    private void state(String s) {
        if (listener != null) listener.onState(s);
    }

    private void line(String s) {
        if (listener != null) listener.onLine(s);
    }
}
