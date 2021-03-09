/*****************************************************************************
 * Free42 -- an HP-42S calculator simulator
 * Copyright (C) 2004-2021  Thomas Okken
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see http://www.gnu.org/licenses/.
 *****************************************************************************/

package com.thomasokken.free42;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.RandomAccessFile;
import java.io.UnsupportedEncodingException;
import java.lang.reflect.Method;
import java.nio.IntBuffer;
import java.text.DateFormat;
import java.text.DecimalFormat;
import java.text.DecimalFormatSymbols;
import java.util.ArrayList;
import java.util.Calendar;
import java.util.List;

import android.Manifest;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Rect;
import android.hardware.GeomagneticField;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.location.Criteria;
import android.location.Location;
import android.location.LocationListener;
import android.location.LocationManager;
import android.media.AudioManager;
import android.media.SoundPool;
import android.net.Uri;
import android.os.BatteryManager;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.Looper;
import android.os.Vibrator;
import android.support.annotation.NonNull;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.support.v4.content.FileProvider;
import android.text.SpannableString;
import android.text.method.LinkMovementMethod;
import android.text.util.Linkify;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.RelativeLayout;
import android.widget.ScrollView;
import android.widget.TextView;

/**
 * This Activity class contains most of the Free42 'shell' functionality;
 * the skin-specific code is separated into the SkinLayout class.
 * This class works in conjunction with free42glue.cc, which is the JNI-
 * based interface to the Free42 'core' functionality (the core is
 * C++ and porting it to Java is not practical, hence the use of JNI).
 */
@SuppressWarnings("deprecation")
public class Free42Activity extends Activity {

    public static final String[] builtinSkinNames = new String[] { "Standard", "Landscape" };
    
    private static final int SHELL_VERSION = 18;
    
    private static final int PRINT_BACKGROUND_COLOR = Color.LTGRAY;
    
    private static final int MY_PERMISSIONS_REQUEST_ACCESS_FINE_LOCATION = 1;
    
    public static Free42Activity instance;
    
    static {
        System.loadLibrary("free42");
    }
    
    private CalcView calcView;
    private SkinLayout skin;
    private View printView;
    private PrintPaperView printPaperView;
    private ScrollView printScrollView;
    private boolean printViewShowing;
    private PreferencesDialog preferencesDialog;
    private AlertDialog mainMenuDialog;
    private AlertDialog programImportExportMenuDialog;
    private Handler mainHandler;
    private boolean alwaysOn;
    
    private SoundPool soundPool;
    private int[] soundIds;
    
    // Streams for reading and writing the state file
    private PositionTrackingInputStream stateFileInputStream;
    private OutputStream stateFileOutputStream;
    
    // Show "States" dialog if invoked after state import; this
    // will have the name of the most recently imported state.
    // If this is null, don't do anything.
    public String importedState;

    // FileImportActivity can't import programs on its own,
    // since that requires Free42Activity to be loaded. This
    // is where the former communicates the name of the file
    // to import to the latter.
    public String importedProgram;
    
    private int ckey;
    private boolean timeout3_active;
    private boolean quit_flag;
    
    private boolean low_battery;
    private BroadcastReceiver lowBatteryReceiver;

    // Persistent state
    private int orientation = 0; // 0=portrait, 1=landscape
    private String[] skinName = new String[] { builtinSkinNames[0], builtinSkinNames[0] };
    private String[] externalSkinName = new String[2];
    private boolean[] skinSmoothing = new boolean[2];
    private boolean[] displaySmoothing = new boolean[2];
    private boolean[] maintainSkinAspect = new boolean[2];
    private String coreName;

    private boolean alwaysRepaintFullDisplay = false;
    private int keyClicksLevel = 3;
    private int keyVibration = 0;
    private int preferredOrientation = ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED;
    private int style = 0;
    
    private final Runnable repeaterCaller = new Runnable() { public void run() { repeater(); } };
    private final Runnable timeout1Caller = new Runnable() { public void run() { timeout1(); } };
    private final Runnable timeout2Caller = new Runnable() { public void run() { timeout2(); } };
    private final Runnable timeout3Caller = new Runnable() { public void run() { timeout3(); } };

    private static class PositionTrackingInputStream extends InputStream {
        private InputStream stream;
        private int pos;
        public PositionTrackingInputStream(InputStream stream) {
            this.stream = stream;
            pos = 0;
        }
        public int getPosition() {
            return pos;
        }
        @Override
        public int read() throws IOException {
            if (pos == -1)
                throw new IOException();
            try {
                pos++;
                return stream.read();
            } catch (IOException e) {
                pos = -1;
                throw e;
            }
        }
        @Override
        public int read(byte[] buf) throws IOException {
            if (pos == -1)
                throw new IOException();
            try {
                pos += buf.length;
                return stream.read(buf);
            } catch (IOException e) {
                pos = -1;
                throw e;
            }
        }
        @Override
        public int read(byte[] buf, int offset, int length) throws IOException {
            if (pos == -1)
                throw new IOException();
            try {
                pos += buf.length;
                return stream.read(buf, offset, length);
            } catch (IOException e) {
                pos = -1;
                throw e;
            }
        }
        @Override
        public boolean markSupported() {
            return false;
        }
        @Override
        public int available() throws IOException {
            return stream.available();
        }
        @Override
        public void close() throws IOException {
            stream.close();
        }
        @Override
        public void mark(int readlimit) {
            throw new UnsupportedOperationException();
        }
        @Override
        public void reset() {
            throw new UnsupportedOperationException();
        }
        @Override
        public long skip(long n) throws IOException {
            return stream.skip(n);
        }
    }
    
    ///////////////////////////////////////////////////////
    ///// Top-level code to interface with Android UI /////
    ///////////////////////////////////////////////////////
    
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        instance = this;

        Intent intent = getIntent();
        importedState = intent.getStringExtra("importedState");
        importedProgram = intent.getStringExtra("importedProgram");
        
        int init_mode;
        IntHolder version = new IntHolder();
        String coreFileName = null;
        int coreFileOffset = 0;
        try {
            stateFileInputStream = new PositionTrackingInputStream(openFileInput("state"));
        } catch (FileNotFoundException e) {
            stateFileInputStream = null;
        }
        if (stateFileInputStream != null) {
            if (read_shell_state(version))
                init_mode = 1;
            else {
                init_shell_state(-1);
                init_mode = 2;
            }
        } else {
            init_shell_state(-1);
            init_mode = 0;
        }
        if (init_mode == 1) {
            if (version.value > 25) {
                coreFileName = getFilesDir() + "/" + coreName + ".p42";
            } else {
                coreFileName = getFilesDir() + "/state";
                coreFileOffset = stateFileInputStream.getPosition();
            }
            try {
                stateFileInputStream.close();
            } catch (IOException e) {}
        }  else {
            // The shell state was missing or corrupt, but there
            // may still be a valid core state...
            coreFileName = getFilesDir() + "/" + coreName + ".p42";
            if (new File(coreFileName).isFile()) {
                // Core state "Untitled.p42" exists; let's try to read it
                init_mode = 1;
                version.value = 26;
            }
        }

        setAlwaysRepaintFullDisplay(alwaysRepaintFullDisplay);
        if (alwaysOn)
            getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        if (style == 1)
            setTheme(android.R.style.Theme_NoTitleBar_Fullscreen);
        else if (style == 2) {
            try {
                Method m = View.class.getMethod("setSystemUiVisibility", int.class);
                m.invoke(getWindow().getDecorView(), PreferencesDialog.immersiveModeFlags);
            } catch (Exception e) {}
        }
        
        Configuration conf = getResources().getConfiguration();
        orientation = conf.orientation == Configuration.ORIENTATION_LANDSCAPE ? 1 : 0;
        
        mainHandler = new Handler();
        calcView = new CalcView(this);
        setContentView(calcView);

        LayoutInflater inflater = (LayoutInflater) getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        printView = inflater.inflate(R.layout.print_view, null);
        Button button = (Button) printView.findViewById(R.id.advB);
        button.setOnClickListener(new View.OnClickListener() {
            public void onClick(View view) {
                doPrintAdv();
            }
        });
        button = (Button) printView.findViewById(R.id.copyTxtB);
        button.setOnClickListener(new View.OnClickListener() {
            public void onClick(View view) {
                printPaperView.copyAsText();
            }
        });
        /*
        button = (Button) printView.findViewById(R.id.copyImgB);
        button.setOnClickListener(new View.OnClickListener() {
            public void onClick(View view) {
                printPaperView.copyAsImage();
            }
        });
        */
        button = (Button) printView.findViewById(R.id.shareB);
        button.setOnClickListener(new View.OnClickListener() {
            public void onClick(View view) {
                printPaperView.share();
            }
        });
        button = (Button) printView.findViewById(R.id.clearB);
        button.setOnClickListener(new View.OnClickListener() {
            public void onClick(View view) {
                printPaperView.clear();
            }
        });
        button = (Button) printView.findViewById(R.id.doneB);
        button.setOnClickListener(new View.OnClickListener() {
            public void onClick(View view) {
                doFlipCalcPrintout();
            }
        });
        printPaperView = new PrintPaperView(this);
        printScrollView = (ScrollView) printView.findViewById(R.id.printScrollView);
        printScrollView.setBackgroundColor(PRINT_BACKGROUND_COLOR);
        printScrollView.addView(printPaperView);
        
        skin = null;
        if (skinName[orientation].length() == 0 && externalSkinName[orientation].length() > 0) {
            try {
                skin = new SkinLayout(this, externalSkinName[orientation], skinSmoothing[orientation], displaySmoothing[orientation], maintainSkinAspect[orientation]);
            } catch (IllegalArgumentException e) {}
        }
        if (skin == null) {
            try {
                skin = new SkinLayout(this, skinName[orientation], skinSmoothing[orientation], displaySmoothing[orientation], maintainSkinAspect[orientation]);
            } catch (IllegalArgumentException e) {}
        }
        if (skin == null) {
            try {
                skin = new SkinLayout(this, builtinSkinNames[0], skinSmoothing[orientation], displaySmoothing[orientation], maintainSkinAspect[orientation]);
            } catch (IllegalArgumentException e) {
                // This one should never fail; we're loading a built-in skin.
            }
        }
        calcView.updateScale();

        nativeInit();
        core_init(init_mode, version.value, coreFileName, coreFileOffset);

        lowBatteryReceiver = new BroadcastReceiver() {
            public void onReceive(Context ctx, Intent intent) {
                low_battery = Intent.ACTION_BATTERY_LOW.equals(intent.getAction());
                Rect inval = skin.update_annunciators(-1, -1, -1, -1, low_battery ? 1 : 0, -1, -1);
                if (inval != null)
                    calcView.postInvalidateScaled(inval.left, inval.top, inval.right, inval.bottom);
            }
        };
        IntentFilter iff = new IntentFilter();
        iff.addAction(Intent.ACTION_BATTERY_LOW);
        iff.addAction(Intent.ACTION_BATTERY_OKAY);
        registerReceiver(lowBatteryReceiver, iff);
        
        if (preferredOrientation != this.getRequestedOrientation())
            setRequestedOrientation(preferredOrientation);

        soundPool = new SoundPool(1, AudioManager.STREAM_SYSTEM, 0);
        int[] soundResourceIds = {
                R.raw.tone0, R.raw.tone1, R.raw.tone2, R.raw.tone3, R.raw.tone4,
                R.raw.tone5, R.raw.tone6, R.raw.tone7, R.raw.tone8, R.raw.tone9,
                R.raw.squeak,
                R.raw.click1, R.raw.click2, R.raw.click3, R.raw.click4, R.raw.click5,
                R.raw.click6, R.raw.click7, R.raw.click8, R.raw.click9
            };
        soundIds = new int[soundResourceIds.length];
        for (int i = 0; i < soundResourceIds.length; i++)
            soundIds[i] = soundPool.load(this, soundResourceIds[i], 1);
    }
    
    @Override
    protected void onStart() {
        // Check battery level -- this is necessary because the ACTTON_BATTERY_LOW
        // and ACTION_BATTERY_OKAY intents are not "sticky", i.e., we get those
        // notifications only when that status *changes*; we don't get any indication
        // of what that status *is* when the app is launched (or resumed?).
        IntentFilter ifilter = new IntentFilter(Intent.ACTION_BATTERY_CHANGED);
        Intent batteryStatus = registerReceiver(null, ifilter);
        assert batteryStatus != null;
        int level = batteryStatus.getIntExtra(BatteryManager.EXTRA_LEVEL, -1);
        int scale = batteryStatus.getIntExtra(BatteryManager.EXTRA_SCALE, -1);
        if (low_battery)
            low_battery = level * 100 < scale * 20;
        else
            low_battery = level * 100 <= scale * 15;
        Rect inval = skin.update_annunciators(-1, -1, -1, -1, low_battery ? 1 : 0, -1, -1);
        if (inval != null)
            calcView.postInvalidateScaled(inval.left, inval.top, inval.right, inval.bottom);

        if (core_powercycle())
            startRunner();
        
        super.onStart();

        String impTemp = importedState;
        importedState = null;
        if (impTemp != null)
            doStates(impTemp);
        impTemp = importedProgram;
        importedProgram = null;
        if (impTemp != null) {
            doImport2(impTemp);
            new File(impTemp).delete();
        }
    }
    
    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (style == 2) {
            try {
                Method m = View.class.getMethod("setSystemUiVisibility", int.class);
                m.invoke(getWindow().getDecorView(), PreferencesDialog.immersiveModeFlags);
            } catch (Exception e) {}
        }
    }

    @Override
    protected void onStop() {
        // Write shell state
        stateFileOutputStream = null;
        try {
            stateFileOutputStream = openFileOutput("state", Context.MODE_PRIVATE);
            write_shell_state();
            stateFileOutputStream.close();
        } catch (Exception e) {
            if (stateFileOutputStream != null)
                try {
                    stateFileOutputStream.close();
                } catch (IOException e2) {}
        }

        // Write core state
        core_save_state(getFilesDir() + "/" + coreName + ".p42");

        printPaperView.dump();
        if (printTxtStream != null) {
            try {
                printTxtStream.close();
            } catch (IOException e) {}
            printTxtStream = null;
        }
        if (printGifFile != null) {
            try {
                ShellSpool.shell_finish_gif(printGifFile);
            } catch (IOException e) {}
            try {
                printGifFile.close();
            } catch (IOException e) {}
            printGifFile = null;
        }
        super.onStop();
    }
    
    @Override
    protected void onDestroy() {
        // core_cleanup();
        if (lowBatteryReceiver != null) {
            unregisterReceiver(lowBatteryReceiver);
            lowBatteryReceiver = null;
        }
        super.onDestroy();
    }
    
    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (keyCode == KeyEvent.KEYCODE_BACK
                && event.getRepeatCount() == 0) {
            event.startTracking();
            return true;
        }
        return super.onKeyDown(keyCode, event);
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        if (keyCode == KeyEvent.KEYCODE_BACK && event.isTracking()
                && !event.isCanceled()) {
            if (printViewShowing) {
                doFlipCalcPrintout();
                return true;
            } else {
                return super.onKeyUp(keyCode, event);
            }
        }
        return super.onKeyUp(keyCode, event);
    }
    
    @Override
    public void onConfigurationChanged(Configuration newConf) {
        super.onConfigurationChanged(newConf);
        orientation = newConf.orientation == Configuration.ORIENTATION_LANDSCAPE ? 1 : 0;
        boolean[] ann_state = skin.getAnnunciators();
        SkinLayout newSkin = null;
        if (skinName[orientation].length() == 0 && externalSkinName[orientation].length() > 0) {
            try {
                newSkin = new SkinLayout(this, externalSkinName[orientation], skinSmoothing[orientation], displaySmoothing[orientation], maintainSkinAspect[orientation], ann_state);
            } catch (IllegalArgumentException e) {}
        }
        if (newSkin == null) {
            try {
                newSkin = new SkinLayout(this, skinName[orientation], skinSmoothing[orientation], displaySmoothing[orientation], maintainSkinAspect[orientation], ann_state);
            } catch (IllegalArgumentException e) {}
        }
        if (newSkin == null) {
            try {
                newSkin = new SkinLayout(this, builtinSkinNames[0], skinSmoothing[orientation], displaySmoothing[orientation], maintainSkinAspect[orientation], ann_state);
            } catch (IllegalArgumentException e) {
                // This one should never fail; we're loading a built-in skin.
            }
        }
        if (newSkin != null)
            skin = newSkin;
        calcView.updateScale();
        calcView.invalidate();
        core_repaint_display();
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        // ignore
    }

    private void startRunner() {
        boolean keep_running = core_keydown(0, null, null, false);
        if (keep_running && !quit_flag) {
            Handler h = new Handler();
            h.postDelayed(
                new Runnable() {
                    @Override
                    public void run() {
                        startRunner();
                    }
                }, 0);
        }
    }

    private void cancelRepeaterAndTimeouts1And2() {
        mainHandler.removeCallbacks(repeaterCaller);
        mainHandler.removeCallbacks(timeout1Caller);
        mainHandler.removeCallbacks(timeout2Caller);
    }
    
    private void cancelTimeout3() {
        mainHandler.removeCallbacks(timeout3Caller);
        timeout3_active = false;
    }

    private void postMainMenu() {
        if (mainMenuDialog == null) {
            AlertDialog.Builder builder = new AlertDialog.Builder(this);
            builder.setTitle("Main Menu");
            List<String> itemsList = new ArrayList<String>();
            itemsList.add("Show Print-Out");
            itemsList.add("Program Import & Export");
            itemsList.add("States");
            itemsList.add("Preferences");
            itemsList.add("Select Skin");
            itemsList.add("Skin: Other...");
            itemsList.add("Copy");
            itemsList.add("Paste");
            itemsList.add("About Plus42");
            itemsList.add("Cancel");
            builder.setItems(itemsList.toArray(new String[itemsList.size()]),
                    new DialogInterface.OnClickListener() {
                        public void onClick(DialogInterface dialog, int which) {
                            mainMenuItemSelected(which);
                        }
                    });
            mainMenuDialog = builder.create();
        }
        mainMenuDialog.show();
    }

    private void mainMenuItemSelected(int which) {
        switch (which) {
            case 0:
                doFlipCalcPrintout();
                return;
            case 1:
                postProgramImportExportMenu();
                return;
            case 2:
                doStates(null);
                return;
            case 3:
                doPreferences();
                return;
            case 4:
                doSelectSkin();
                break;
            case 5:
                doSkinOther();
                break;
            case 6:
                doCopy();
                return;
            case 7:
                doPaste();
                return;
            case 8:
                doAbout();
                return;
            // default: Cancel; do nothing
        }
    }

    private void postProgramImportExportMenu() {
        if (programImportExportMenuDialog == null) {
            AlertDialog.Builder builder = new AlertDialog.Builder(this);
            builder.setTitle("Import & Export Menu");
            List<String> itemsList = new ArrayList<String>();
            itemsList.add("Import Programs");
            itemsList.add("Export Programs");
            itemsList.add("Share Programs");
            itemsList.add("Back");
            itemsList.add("Cancel");
            builder.setItems(itemsList.toArray(new String[itemsList.size()]),
                    new DialogInterface.OnClickListener() {
                        public void onClick(DialogInterface dialog, int which) {
                            programImportExportMenuItemSelected(which);
                        }
                    });
            programImportExportMenuDialog = builder.create();
        }
        programImportExportMenuDialog.show();
    }

    private void programImportExportMenuItemSelected(int which) {
        switch (which) {
            case 0:
                doImport();
                return;
            case 1:
                doExport(false);
                return;
            case 2:
                doExport(true);
                return;
            case 3:
                postMainMenu();
                return;
            // default: Cancel; do nothing
        }
    }
    
    private void doSelectSkin() {
        SkinSelectDialog ssd = new SkinSelectDialog(this);
        ssd.setListener(new SkinSelectDialog.Listener() {
            @Override
            public void skinSelected(String skinName) {
                doSelectSkin(skinName);
            }
        });
        ssd.show();
    }

    private void doSkinOther() {
        if (!checkStorageAccess())
            return;
        FileSelectionDialog fsd = new FileSelectionDialog(this, new String[] { "layout", "*" });
        if (externalSkinName[orientation].length() > 0)
            fsd.setPath(externalSkinName[orientation] + ".layout");
        fsd.setOkListener(new FileSelectionDialog.OkListener() {
            public void okPressed(String path) {
                if (path.endsWith(".layout"))
                    doSelectSkin(path.substring(0, path.length() - 7));
            }
        });
        fsd.show();
    }

    public static String getSelectedSkin() {
        return instance.skinName[instance.orientation];
    }

    public static String[] getSelectedSkins() {
        return instance.skinName;
    }

    public static String getSelectedState() {
        return instance.coreName;
    }

    public static void setSelectedState(String stateName) {
        instance.coreName = stateName;
    }

    public static void switchToState(String stateName) {
        instance.switchToState2(stateName);
    }

    private void switchToState2(String stateName) {
        if (!stateName.equals(coreName)) {
            String oldFileName = getFilesDir() + "/" + coreName + ".p42";
            core_save_state(oldFileName);
        }
        core_cleanup();
        coreName = stateName;
        String newFileName = getFilesDir() + "/" + coreName + ".p42";
        core_init(1, 26, newFileName, 0);
        if (core_powercycle())
            startRunner();
    }

    public static void saveStateAs(String fileName) {
        instance.core_save_state(fileName);
    }

    private void doCopy() {
        android.text.ClipboardManager clip = (android.text.ClipboardManager) getSystemService(CLIPBOARD_SERVICE);
        clip.setText(core_copy());
    }
    
    private void doPaste() {
        android.text.ClipboardManager clip = (android.text.ClipboardManager) getSystemService(CLIPBOARD_SERVICE);
        if (clip.hasText())
            core_paste(clip.getText().toString());
    }
    
    private void doFlipCalcPrintout() {
        printViewShowing = !printViewShowing;
        setContentView(printViewShowing ? printView : calcView);
    }
    
    private void doImport() {
        if (!checkStorageAccess())
            return;
        FileSelectionDialog fsd = new FileSelectionDialog(this, new String[] { "raw", "*" });
        fsd.setOkListener(new FileSelectionDialog.OkListener() {
            public void okPressed(String path) {
                doImport2(path);
            }
        });
        fsd.show();
    }
    
    private void doImport2(String path) {
        core_import_programs(path);
        redisplay();
    }
    
    private boolean[] selectedProgramIndexes;
    private String[] programNames;
    private boolean exportShare;

    public static void showAlert(String message) {
        instance.alert(message);
    }

    private void alert(String message) {
        runOnUiThread(new Alerter(message));
    }
    
    private class Alerter implements Runnable {
        private String message;
        public Alerter(String message) {
            this.message = message;
        }
        public void run() {
            AlertDialog.Builder builder = new AlertDialog.Builder(Free42Activity.this);
            builder.setMessage(message);
            builder.setPositiveButton("OK", null);
            builder.create().show();
        }
    }

    private void doExport(boolean share) {
        if (!share && !checkStorageAccess())
            return;
        exportShare = share;
        programNames = core_list_programs();
        selectedProgramIndexes = new boolean[programNames.length];
        
        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        builder.setTitle("Select Programs");
        builder.setMultiChoiceItems(programNames, selectedProgramIndexes, new DialogInterface.OnMultiChoiceClickListener() {
            public void onClick(DialogInterface dialog, int which, boolean isChecked) {
                // I don't have to do anything here; the only reason why
                // I create this listener is because if I pass 'null'
                // instead, the selectedProgramIndexes array never gets
                // updated.
            }
        });
        DialogInterface.OnClickListener listener = new DialogInterface.OnClickListener() {
            public void onClick(DialogInterface dialog, int which) {
                doProgramSelectionClick(dialog, which);
            }
        };
        builder.setPositiveButton("OK", listener);
        builder.setNegativeButton("Cancel", null);
        builder.create().show();
    }

    private void doStates(String selectedState) {
        StatesDialog sd = new StatesDialog(this, selectedState);
        sd.show();
    }
    
    private void doProgramSelectionClick(DialogInterface dialog, int which) {
        if (which == DialogInterface.BUTTON_POSITIVE) {
            String name = null;
            for (int i = 0; i < selectedProgramIndexes.length; i++)
                if (selectedProgramIndexes[i]) {
                    name = programNames[i];
                    break;
                }
            if (name != null) {
                if (exportShare) {
                    doShare();
                } else {
                    FileSelectionDialog fsd = new FileSelectionDialog(this, new String[]{"raw", "*"});
                    fsd.setOkListener(new FileSelectionDialog.OkListener() {
                        public void okPressed(String path) {
                            doExport2(path);
                        }
                    });
                    if (name.startsWith("\"")) {
                        int q = name.indexOf('"', 1);
                        if (q != -1)
                            name = name.substring(1, q).replaceAll("[/\n]", "_") + ".raw";
                        else
                            name = "Untitled.raw";
                    } else {
                        name = "Untitled.raw";
                    }
                    fsd.setPath(name);
                    fsd.show();
                }
            }
        }
        dialog.dismiss();
    }
    
    private void doExport2(String path) {
        int n = 0;
        for (int i = 0; i < selectedProgramIndexes.length; i++)
            if (selectedProgramIndexes[i])
                n++;
        int[] selection = new int[n];
        n = 0;
        for (int i = 0; i < selectedProgramIndexes.length; i++)
            if (selectedProgramIndexes[i])
                selection[n++] = i;
        core_export_programs(selection, path);
    }

    private void doShare() {
        int n = -1;
        int m = 0;
        for (int i = 0; i < selectedProgramIndexes.length; i++) {
            if (selectedProgramIndexes[i]) {
                m++;
                if (m == 1)
                    n = i;
                else if (m == 2)
                    break;
            }
        }
        if (n == -1)
            // Should never happen
            return;
        String name = programNames[n];
        if (name.charAt(0) == '"') {
            int q = name.indexOf('"', 1);
            name = name.substring(1, q);
        } else
            name = "Untitled";
        File cacheDir = new File(getFilesDir(), "cache");
        cacheDir.mkdir();
        // Remove old *.raw files, so we don't use an ever-growing
        // chunk of space for these files that really should be
        // temporary
        File[] cacheFiles = cacheDir.listFiles();
        for (File f : cacheFiles) {
            if (f.getName().endsWith(".raw"))
                f.delete();
        }
        // OK, now write the selected programs to a new raw file
        String path = cacheDir + "/" + name + ".raw";
        doExport2(path);
        // And now share that file
        Intent intent = new Intent(Intent.ACTION_SEND);
        File file = new File(path);
        Uri uri = FileProvider.getUriForFile(this, getPackageName() + ".fileprovider", file);
        intent.setType("*/*");
        intent.putExtra(Intent.EXTRA_STREAM, uri);
        startActivity(Intent.createChooser(intent, "Share Plus42 Program" + (m == 1 ? "" : "s") + " Using"));
    }

    private void doSelectSkin(String skinName) {
        try {
            boolean[] annunciators = skin.getAnnunciators();
            skin = new SkinLayout(this, skinName, skinSmoothing[orientation], displaySmoothing[orientation], maintainSkinAspect[orientation], annunciators);
            if (skinName.startsWith("/")) {
                externalSkinName[orientation] = skinName;
                this.skinName[orientation] = "";
            } else
                this.skinName[orientation] = skinName;
            calcView.updateScale();
            calcView.invalidate();
            core_repaint_display();
        } catch (IllegalArgumentException e) {
            shell_beeper(1835, 125);
        }
    }
    
    private void doPreferences() {
        if (preferencesDialog == null) {
            preferencesDialog = new PreferencesDialog(this);
            preferencesDialog.setOkListener(new PreferencesDialog.OkListener() {
                public void okPressed() {
                    doPreferencesOk();
                }
            });
        }
        
        CoreSettings cs = new CoreSettings();
        getCoreSettings(cs);
        preferencesDialog.setSingularMatrixError(cs.matrix_singularmatrix);
        preferencesDialog.setMatrixOutOfRange(cs.matrix_outofrange);
        preferencesDialog.setAutoRepeat(cs.auto_repeat);
        preferencesDialog.setAllowBigStack(cs.allow_big_stack);
        preferencesDialog.setAlwaysOn(shell_always_on(-1));
        preferencesDialog.setKeyClicks(keyClicksLevel);
        preferencesDialog.setKeyVibration(keyVibration);
        preferencesDialog.setOrientation(preferredOrientation);
        preferencesDialog.setStyle(style);
        preferencesDialog.setDisplayFullRepaint(alwaysRepaintFullDisplay);
        preferencesDialog.setMaintainSkinAspect(maintainSkinAspect[orientation]);
        preferencesDialog.setSkinSmoothing(skinSmoothing[orientation]);
        preferencesDialog.setDisplaySmoothing(displaySmoothing[orientation]);
        preferencesDialog.setPrintToText(ShellSpool.printToTxt);
        preferencesDialog.setPrintToTextFileName(ShellSpool.printToTxtFileName);
        preferencesDialog.setPrintToGif(ShellSpool.printToGif);
        preferencesDialog.setPrintToGifFileName(ShellSpool.printToGifFileName);
        preferencesDialog.setMaxGifHeight(ShellSpool.maxGifHeight);
        preferencesDialog.show();
    }
        
    private void doPreferencesOk() {
        CoreSettings cs = new CoreSettings();
        getCoreSettings(cs);
        cs.matrix_singularmatrix = preferencesDialog.getSingularMatrixError();
        cs.matrix_outofrange = preferencesDialog.getMatrixOutOfRange();
        cs.auto_repeat = preferencesDialog.getAutoRepeat();
        boolean oldBigStack = cs.allow_big_stack;
        cs.allow_big_stack = preferencesDialog.getAllowBigStack();
        putCoreSettings(cs);
        if (oldBigStack != cs.allow_big_stack)
            core_update_allow_big_stack();
        shell_always_on(preferencesDialog.getAlwaysOn() ? 1 : 0);
        keyClicksLevel = preferencesDialog.getKeyClicks();
        keyVibration = preferencesDialog.getKeyVibration();
        int oldOrientation = preferredOrientation;
        preferredOrientation = preferencesDialog.getOrientation();
        style = preferencesDialog.getStyle();
        alwaysRepaintFullDisplay = preferencesDialog.getDisplayFullRepaint();
        setAlwaysRepaintFullDisplay(alwaysRepaintFullDisplay);

        ShellSpool.maxGifHeight = preferencesDialog.getMaxGifHeight();

        boolean newPrintEnabled = preferencesDialog.getPrintToText();
        String newFileName = preferencesDialog.getPrintToTextFileName();
        if (printTxtStream != null && (!newPrintEnabled || !newFileName.equals(ShellSpool.printToTxtFileName))) {
            try {
                printTxtStream.close();
            } catch (IOException e) {}
            printTxtStream = null;
        }
        ShellSpool.printToTxt = newPrintEnabled;
        ShellSpool.printToTxtFileName = newFileName;
        
        newPrintEnabled = preferencesDialog.getPrintToGif();
        newFileName = preferencesDialog.getPrintToGifFileName();
        if (printGifFile != null && (!newPrintEnabled || !newFileName.equals(ShellSpool.printToGifFileName))) {
            try {
                ShellSpool.shell_finish_gif(printGifFile);
            } catch (IOException e) {}
            try {
                printGifFile.close();
            } catch (IOException e) {}
            printGifFile = null;
            gif_seq = 0;
        }
        ShellSpool.printToGif = newPrintEnabled;
        ShellSpool.printToGifFileName = newFileName;
        
        boolean newMaintainSkinAspect = preferencesDialog.getMaintainSkinAspect();
        if (newMaintainSkinAspect != maintainSkinAspect[orientation]) {
            maintainSkinAspect[orientation] = newMaintainSkinAspect;
            skin.setMaintainSkinAspect(newMaintainSkinAspect);
            calcView.updateScale();
            calcView.invalidate();
        }
        
        boolean newSkinSmoothing = preferencesDialog.getSkinSmoothing();
        boolean newDisplaySmoothing = preferencesDialog.getDisplaySmoothing();
        if (newSkinSmoothing != skinSmoothing[orientation] || newDisplaySmoothing != displaySmoothing[orientation]) {
            skinSmoothing[orientation] = newSkinSmoothing;
            displaySmoothing[orientation] = newDisplaySmoothing;
            skin.setSmoothing(newSkinSmoothing, newDisplaySmoothing);
            calcView.invalidate();
        }
        
        if (preferredOrientation != oldOrientation)
            setRequestedOrientation(preferredOrientation);
    }
    
    private void doAbout() {
        new AboutDialog(this).show();
    }
    
    public class AboutDialog extends Dialog {
        private AboutView view;
        
        public AboutDialog(Context context) {
            super(context);
            view = new AboutView(context);
            setContentView(view);
            this.setTitle("About Plus42");
        }
        
        private class AboutView extends RelativeLayout {
            public AboutView(Context context) {
                super(context);
                
                ImageView icon = new ImageView(context);
                icon.setId(1);
                icon.setImageResource(R.mipmap.icon);
                addView(icon);
                
                TextView label1 = new TextView(context);
                label1.setId(2);
                String version = "";
                try {
                    version = " " + getPackageManager().getPackageInfo(getPackageName(), 0).versionName;
                } catch (NameNotFoundException e) {}
                label1.setText("Plus42" + version);
                LayoutParams lp = new RelativeLayout.LayoutParams(RelativeLayout.LayoutParams.WRAP_CONTENT, RelativeLayout.LayoutParams.WRAP_CONTENT);
                lp.addRule(RelativeLayout.ALIGN_TOP, icon.getId());
                lp.addRule(RelativeLayout.RIGHT_OF, icon.getId());
                addView(label1, lp);

                TextView label2 = new TextView(context);
                label2.setId(3);
                label2.setText("\u00a9 2004-2021 Thomas Okken");
                lp = new RelativeLayout.LayoutParams(RelativeLayout.LayoutParams.WRAP_CONTENT, RelativeLayout.LayoutParams.WRAP_CONTENT);
                lp.addRule(RelativeLayout.ALIGN_LEFT, label1.getId());
                lp.addRule(RelativeLayout.BELOW, label1.getId());
                addView(label2, lp);

                TextView label3 = new TextView(context);
                label3.setId(4);
                SpannableString s = new SpannableString("https://thomasokken.com/plus42/");
                Linkify.addLinks(s, Linkify.WEB_URLS);
                label3.setText(s);
                label3.setMovementMethod(LinkMovementMethod.getInstance());
                lp = new RelativeLayout.LayoutParams(RelativeLayout.LayoutParams.WRAP_CONTENT, RelativeLayout.LayoutParams.WRAP_CONTENT);
                lp.addRule(RelativeLayout.ALIGN_LEFT, label2.getId());
                lp.addRule(RelativeLayout.BELOW, label2.getId());
                addView(label3, lp);

                TextView label4 = new TextView(context);
                label4.setId(5);
                s = new SpannableString("https://thomasokken.com/plus42/#doc");
                Linkify.addLinks(s, Linkify.WEB_URLS);
                label4.setText(s);
                label4.setMovementMethod(LinkMovementMethod.getInstance());
                lp = new RelativeLayout.LayoutParams(RelativeLayout.LayoutParams.WRAP_CONTENT, RelativeLayout.LayoutParams.WRAP_CONTENT);
                lp.addRule(RelativeLayout.ALIGN_LEFT, label3.getId());
                lp.addRule(RelativeLayout.BELOW, label3.getId());
                addView(label4, lp);

                Button okB = new Button(context);
                okB.setId(6);
                okB.setText("   OK   ");
                okB.setOnClickListener(new OnClickListener() {
                    public void onClick(View view) {
                        AboutDialog.this.dismiss();
                    }
                });
                lp = new RelativeLayout.LayoutParams(RelativeLayout.LayoutParams.WRAP_CONTENT, RelativeLayout.LayoutParams.WRAP_CONTENT);
                lp.addRule(RelativeLayout.BELOW, label4.getId());
                lp.addRule(RelativeLayout.CENTER_HORIZONTAL);
                addView(okB, lp);

            }
        }
    }
    /**
     * This class is calculator view used by the Free42 Activity.
     * Note that most of the heavy lifting takes place in the
     * Activity, not here.
     */
    private class CalcView extends View {
        
        private int width, height;
        private float hScale, vScale;
        private int hOffset, vOffset;
        private boolean possibleMenuEvent = false;

        public CalcView(Context context) {
            super(context);
        }
        
        public void updateScale() {
            vScale = ((float) height) / skin.getHeight();
            hScale = ((float) width) / skin.getWidth();
            hOffset = vOffset = 0;
            if (skin.getMaintainSkinAspect()) {
                if (hScale > vScale) {
                    hScale = vScale = ((float) height) / skin.getHeight();
                    hOffset = (int) ((width - skin.getWidth() * hScale) / 2);
                } else {
                    hScale = vScale = ((float) width) / skin.getWidth();
                    vOffset = (int) ((height - skin.getHeight() * vScale) / 2);
                }
            }
        }

        @Override
        protected void onSizeChanged(int w, int h, int oldw, int oldh) {
            width = w;
            height = h;
            updateScale();
        }

        @Override
        protected void onDraw(Canvas canvas) {
            canvas.translate(hOffset, vOffset);
            canvas.scale(hScale, vScale);
            skin.repaint(canvas);
        }
        
        @SuppressLint("ClickableViewAccessibility")
        @Override
        public boolean onTouchEvent(MotionEvent e) {
            int what = e.getAction();
            if (what != MotionEvent.ACTION_DOWN && what != MotionEvent.ACTION_UP)
                return true;
            
            cancelRepeaterAndTimeouts1And2();
            
            if (what == MotionEvent.ACTION_DOWN) {
                int x = (int) ((e.getX() - hOffset) / hScale);
                int y = (int) ((e.getY() - vOffset) / vScale);
                IntHolder skeyHolder = new IntHolder();
                IntHolder ckeyHolder = new IntHolder();
                skin.find_key(core_menu(), x, y, skeyHolder, ckeyHolder);
                int skey = skeyHolder.value;
                ckey = ckeyHolder.value;
                if (ckey == 0) {
                    if (skin.in_menu_area(x, y))
                        this.possibleMenuEvent = true;
                    return true;
                }
                click();
                Object macroObj = skin.find_macro(ckey);
                if (timeout3_active && (macroObj != null || ckey != 28 /* SHIFT */)) {
                    cancelTimeout3();
                    core_timeout3(false);
                }
                Rect inval = skin.set_active_key(skey);
                if (inval != null)
                    invalidateScaled(inval);
                boolean running;
                BooleanHolder enqueued = new BooleanHolder();
                IntHolder repeat = new IntHolder();
                if (macroObj == null) {
                    // Plain ol' key
                    running = core_keydown(ckey, enqueued, repeat, true);
                } else if (macroObj instanceof String) {
                    // Direct-mapped command
                    String cmd = (String) macroObj;
                    running = core_keydown_command(cmd, enqueued, repeat, true);
                } else {
                    byte[] macro = (byte[]) macroObj;
                    boolean one_key_macro = macro.length == 1 || (macro.length == 2 && macro[0] == 28);
                    if (!one_key_macro)
                        skin.set_display_enabled(false);
                    for (int i = 0; i < macro.length - 1; i++) {
                        core_keydown(macro[i] & 255, enqueued, repeat, true);
                        if (!enqueued.value)
                            core_keyup();
                    }
                    running = core_keydown(macro[macro.length - 1] & 255, enqueued, repeat, true);
                    if (!one_key_macro)
                        skin.set_display_enabled(true);
                }
                if (running)
                    startRunner();
                else {
                    if (repeat.value != 0)
                        mainHandler.postDelayed(repeaterCaller, repeat.value == 1 ? 1000 : 500);
                    else if (!enqueued.value)
                        mainHandler.postDelayed(timeout1Caller, 250);
                }
            } else {
                if (possibleMenuEvent) {
                    possibleMenuEvent = false;
                    int x = (int) ((e.getX() - hOffset) / hScale);
                    int y = (int) ((e.getY() - vOffset) / vScale);
                    if (skin.in_menu_area(x, y))
                        Free42Activity.this.postMainMenu();
                }
                ckey = 0;
                Rect inval = skin.set_active_key(-1);
                if (inval != null)
                    invalidateScaled(inval);
                boolean keep_running = core_keyup();
                if (keep_running)
                    startRunner();
            }
                
            return true;
        }
        
        public void postInvalidateScaled(int left, int top, int right, int bottom) {
            left = (int) Math.floor(((double) left) * hScale + hOffset);
            top = (int) Math.floor(((double) top) * vScale + vOffset);
            right = (int) Math.ceil(((double) right) * hScale+ hOffset);
            bottom = (int) Math.ceil(((double) bottom) * vScale + vOffset);
            postInvalidate(left - 1, top - 1, right + 2, bottom + 2);
        }

        private void invalidateScaled(Rect inval) {
            inval.left = (int) Math.floor(((double) inval.left) * hScale + hOffset);
            inval.top = (int) Math.floor(((double) inval.top) * vScale + vOffset);
            inval.right = (int) Math.ceil(((double) inval.right) * hScale + hOffset);
            inval.bottom = (int) Math.ceil(((double) inval.bottom) * vScale + vOffset);
            inval.inset(-1, -1);
            invalidate(inval);
        }
    }

    /**
     * This class is the print-out view used by the Free42 Activity.
     * Note that most of the heavy lifting takes place in the
     * Activity, not here.
     */
    private class PrintPaperView extends View {
        
        private static final int BYTESPERLINE = 18;
        // Certain devices have trouble with LINES = 16384; the print-out view collapses.
        // No idea how to detect this behavior, so unclear how to work around it.
        // Playing safe by making the print-out buffer smaller.
        // private static final int LINES = 16384;
        private static final int LINES = 8192;
        // The text buffer is sized to hold as many lines as the graphics buffer,
        // plus two, plus one extra byte. Text lines are stored as a length byte
        // followed by as many bytes, for a maximum of 25 bytes per line.
        // PRLCD lines are stored as just one byte, 0xff, and copyAsText must
        // then get the actual pixels from the graphics buffer.
        private static final int TEXT_SIZE = ((LINES + 27) / 9) * 25 + 1;
        
        private byte[] buffer = new byte[LINES * BYTESPERLINE];
        private int top, bottom;
        private int printHeight;
        private byte[] textBuffer = new byte[TEXT_SIZE];
        private int textTop, textBottom, textPixelHeight;
        private int screenWidth;
        private float scale;

        public PrintPaperView(Context context) {
            super(context);
            InputStream printInputStream = null;
            try {
                printInputStream = openFileInput("print");
                byte[] intBuf = new byte[4];
                if (printInputStream.read(intBuf) != 4)
                    throw new IOException();
                bottom = (intBuf[0] << 24) | ((intBuf[1] & 255) << 16) | ((intBuf[2] & 255) << 8) | (intBuf[3] & 255);
                if (bottom < 0 || bottom > (LINES - 1) * BYTESPERLINE)
                    throw new IOException();
                int n = printInputStream.read(buffer, 0, bottom);
                if (n != bottom)
                    throw new IOException();
                if (printInputStream.read(intBuf) != 4)
                    throw new IOException();
                textBottom = (intBuf[0] << 24) | ((intBuf[1] & 255) << 16) | ((intBuf[2] & 255) << 8) | (intBuf[3] & 255);
                if (textBottom < 0 || textBottom >= TEXT_SIZE)
                    throw new IOException();
                if (printInputStream.read(intBuf) != 4)
                    throw new IOException();
                textPixelHeight = (intBuf[0] << 24) | ((intBuf[1] & 255) << 16) | ((intBuf[2] & 255) << 8) | (intBuf[3] & 255);
                n = printInputStream.read(textBuffer, 0, textBottom);
                if (n != textBottom)
                    throw new IOException();
            } catch (IOException e) {
                bottom = textBottom = textPixelHeight = 0;
            } finally {
                if (printInputStream != null)
                    try {
                        printInputStream.close();
                    } catch (IOException e2) {}
            }
            top = textTop = 0;

            printHeight = bottom / BYTESPERLINE;
            int w = getWindowManager().getDefaultDisplay().getWidth();
            int h = getWindowManager().getDefaultDisplay().getHeight();
            screenWidth = Math.min(w, h);
            scale = screenWidth / 179f;
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            // Pretending our height is never zero, to keep the HTC Aria
            // from throwing a fit. See also the printHeight == 0 case in
            // onDraw().
            setMeasuredDimension(screenWidth, Math.max((int) (printHeight * scale), 1));
        }

        @SuppressLint("DrawAllocation")
        @Override
        protected void onDraw(Canvas canvas) {
            canvas.save();
            canvas.scale(scale, scale);
            Rect clip = canvas.getClipBounds();
            
            if (printHeight == 0) {
                // onMeasure() pretends that our height isn't really zero
                // even if printHeight == 0; this is to prevent the HTC Aria
                // from freaking out. Because of this pretense, we now have
                // to paint something, even though there isn't anything to
                // paint... So we just paint the clip rectangle using the
                // scroll view's background color.
                Paint p = new Paint();
                p.setColor(PRINT_BACKGROUND_COLOR);
                p.setStyle(Paint.Style.FILL);
                canvas.drawRect(clip, p);
                canvas.restore();
                return;
            }
            
            // Construct a temporary bitmap
            int src_x = clip.left;
            int src_y = clip.top;
            int src_width = clip.right - clip.left;
            int src_height = clip.bottom - clip.top;
            Bitmap tmpBitmap = Bitmap.createBitmap(src_width, src_height, Bitmap.Config.ARGB_8888);
            IntBuffer tmpBuffer = IntBuffer.allocate(src_width * src_height);
            int[] tmpArray = tmpBuffer.array();
            for (int y = 0; y < src_height; y++) {
                int yy = y + src_y + (top / BYTESPERLINE);
                if (yy >= LINES)
                    yy -= LINES;
                for (int x = 0; x < src_width; x++) {
                    int xx = x + src_x - 18;
                    if (xx >= 0 && xx < 143) {
                        boolean set = (buffer[yy * BYTESPERLINE + (xx >> 3)] & (1 << (xx & 7))) != 0;
                        tmpArray[y * src_width + x] = set ? Color.BLACK : Color.WHITE;
                    } else
                        tmpArray[y * src_width + x] = Color.WHITE;
                }
            }
            tmpBitmap.copyPixelsFromBuffer(tmpBuffer);
            canvas.drawBitmap(tmpBitmap, new Rect(0, 0, src_width, src_height), clip, new Paint());
            canvas.restore();
        }
        
        @SuppressLint("ClickableViewAccessibility")
        @Override
        public boolean onTouchEvent(MotionEvent e) {
            return true;
        }
        
        @Override
        public void onSizeChanged(int w, int h, int oldw, int oldh) {
            printScrollView.fullScroll(View.FOCUS_DOWN);
        }
        
        private Object invalidatePendingMonitor = new Object();
        private boolean invalidatePending;
        private Object layoutPendingMonitor = new Object();
        private boolean layoutPending;
        
        public void print(byte[] text, byte[] bits, int bytesperline, int x, int y, int width, int height) {
            int oldPrintHeight = printHeight;
            for (int yy = y; yy < y + height; yy++) {
                for (int xx = 0; xx < BYTESPERLINE; xx++)
                    buffer[bottom + xx] = 0;
                for (int xx = x; xx < x + width; xx++) {
                    boolean set = (bits[yy * bytesperline + (xx >> 3)] & (1 << (xx & 7))) != 0;
                    if (set)
                        buffer[bottom + (xx >> 3)] |= 1 << (xx & 7);
                }
                bottom += BYTESPERLINE;
                printHeight++;
                if (bottom >= buffer.length)
                    bottom = 0;
                if (bottom == top) {
                    top += BYTESPERLINE;
                    printHeight--;
                    if (top >= buffer.length)
                        top = 0;
                }
            }

            textBuffer[textBottom++] = (byte) (text == null ? 255 : text.length);
            if (textBottom == TEXT_SIZE)
                textBottom = 0;
            if (text != null) {
                if (textBottom + text.length < TEXT_SIZE) {
                    System.arraycopy(text, 0, textBuffer, textBottom, text.length);
                    textBottom += text.length;
                } else {
                    int part = TEXT_SIZE - textBottom;
                    System.arraycopy(text, 0, textBuffer, textBottom, part);
                    System.arraycopy(text, part, textBuffer, 0, text.length - part);
                    textBottom = text.length - part;
                }
            }
            textPixelHeight += text == null ? 16 : 9;
            while (textPixelHeight > LINES - 1) {
                int tll = textBuffer[textTop] == (byte) 255 ? 16 : 9;
                textPixelHeight -= tll;
                textTop += tll == 16 ? 1 : ((textBuffer[textTop] & 255) + 1);
                if (textTop >= TEXT_SIZE)
                    textTop -= TEXT_SIZE;
            }

            if (printHeight != oldPrintHeight) {
                synchronized (layoutPendingMonitor) {
                    if (!layoutPending) {
                        mainHandler.post(new Runnable() {
                            public void run() {
                                synchronized (layoutPendingMonitor) {
                                    printPaperView.requestLayout();
                                    layoutPending = false;
                                }
                            }
                        });
                        layoutPending = true;
                    }
                }
            } else {
                synchronized (invalidatePendingMonitor) {
                    if (!invalidatePending) {
                        mainHandler.post(new Runnable() {
                            public void run() {
                                synchronized (invalidatePendingMonitor) {
                                    printScrollView.fullScroll(View.FOCUS_DOWN);
                                    printPaperView.postInvalidate();
                                    invalidatePending = false;
                                }
                            }
                        });
                        invalidatePending = true;
                    }
                }
            }
        }

        private String printOutAsText() {
            try {
                ByteArrayOutputStream bos = new ByteArrayOutputStream();

                int len = textBottom - textTop;
                if (len < 0)
                    len += TEXT_SIZE;
                // Calculate effective top, since printout_top can point
                // at a truncated line, and we want to skip those when
                // copying
                int ptop = bottom / BYTESPERLINE - textPixelHeight;
                if (ptop < 0)
                    ptop += LINES;
                int p = textTop;
                int pixel_v = 0;
                byte[] buf = new byte[34];
                while (len > 0) {
                    int z = textBuffer[p++] & 255;
                    if (z == 255) {
                        for (int v = 0; v < 16; v += 2) {
                            for (int vv = 0; vv < 2; vv++) {
                                int V = ptop + pixel_v + v + vv;
                                if (V >= LINES)
                                    V -= LINES;
                                for (int h = 0; h < 17; h++)
                                    buf[vv * 17 + h] = buffer[V * BYTESPERLINE + h];
                            }
                            ShellSpool.shell_spool_bitmap_to_txt(buf, 17, 0, 0, 131, 2, bos);
                        }
                        pixel_v += 16;
                    } else {
                        byte[] tbuf = new byte[z];
                        if (p + z < TEXT_SIZE) {
                            System.arraycopy(textBuffer, p, tbuf, 0, z);
                            ShellSpool.shell_spool_txt(tbuf, bos);
                            p += z;
                        } else {
                            int d = TEXT_SIZE - p;
                            System.arraycopy(textBuffer, p, tbuf, 0, d);
                            System.arraycopy(textBuffer, 0, tbuf, d, z - d);
                            ShellSpool.shell_spool_txt(tbuf, bos);
                            p = z - d;
                        }
                        len -= z;
                        pixel_v += 9;
                    }
                    len--;
                }

                return new String(bos.toByteArray(), "UTF-8").replace("\r", "");
            } catch (IOException e) {
                return null;
            }
        }

        public void copyAsText() {
            String txt = printOutAsText();
            android.text.ClipboardManager clip = (android.text.ClipboardManager) getSystemService(CLIPBOARD_SERVICE);
            clip.setText(txt);
        }

        private String printOutAsImage() {
            // Construct a temporary bitmap
            int src_height = (bottom - top) / BYTESPERLINE;
            if (src_height < 0)
                src_height += LINES;
            Bitmap tmpBitmap = Bitmap.createBitmap(358, 2 * src_height, Bitmap.Config.ARGB_8888);
            IntBuffer tmpBuffer = IntBuffer.allocate(716 * src_height);
            int[] tmpArray = tmpBuffer.array();
            for (int y = 0; y < src_height; y++) {
                int yy = y + (top / BYTESPERLINE);
                if (yy >= LINES)
                    yy -= LINES;
                for (int x = 0; x < 179; x++) {
                    int xx = x - 18;
                    int color;
                    if (xx >= 0 && xx < 143) {
                        boolean set = (buffer[yy * BYTESPERLINE + (xx >> 3)] & (1 << (xx & 7))) != 0;
                        color = set ? Color.BLACK : Color.WHITE;
                    } else
                        color = Color.WHITE;
                    int pos = 716 * y + 2 * x;
                    tmpArray[pos] = tmpArray[pos + 1] = tmpArray[pos + 358] = tmpArray[pos + 359] = color;
                }
            }
            tmpBitmap.copyPixelsFromBuffer(tmpBuffer);
            String cacheDir = getFilesDir() + "/cache";
            new File(cacheDir).mkdir();
            String imageFileName = cacheDir + "/PrintOut.png";
            OutputStream os = null;
            try {
                os = new FileOutputStream(imageFileName);
                tmpBitmap.compress(Bitmap.CompressFormat.PNG, 0, os);
            } catch (IOException e) {
                // Ignore
            } finally {
                if (os != null)
                    try {
                        os.close();
                    } catch (IOException e) {}
            }
            return imageFileName;
        }

        public void copyAsImage() {
            // Not supported by Android?!?
        }

        public void share() {
            if (bottom == top) {
                alert("The print-out is empty.");
                return;
            }
            String text = printOutAsText();
            String pngFileName = printOutAsImage();
            Intent intent = new Intent(Intent.ACTION_SEND);
            intent.putExtra(Intent.EXTRA_TEXT, text);
            Uri uri = FileProvider.getUriForFile(Free42Activity.this, Free42Activity.this.getPackageName() + ".fileprovider", new File(pngFileName));
            intent.setType("*/*");
            intent.putExtra(Intent.EXTRA_STREAM, uri);
            getContext().startActivity(Intent.createChooser(intent, "Share Plus42 Print-Out Using"));
        }

        public void clear() {
            top = bottom = 0;
            textTop = textBottom = textPixelHeight = 0;
            printHeight = 0;
            printPaperView.requestLayout();
        }
        
        public void dump() {
            OutputStream printOutputStream = null;
            try {
                printOutputStream = openFileOutput("print", Context.MODE_PRIVATE);
                int len = bottom - top;
                if (len < 0)
                    len += buffer.length;
                byte[] intBuf = new byte[4];
                intBuf[0] = (byte) (len >> 24);
                intBuf[1] = (byte) (len >> 16);
                intBuf[2] = (byte) (len >> 8);
                intBuf[3] = (byte) len;
                printOutputStream.write(intBuf);
                if (top <= bottom)
                    printOutputStream.write(buffer, top, bottom - top);
                else {
                    printOutputStream.write(buffer, top, buffer.length - top);
                    printOutputStream.write(buffer, 0, bottom);
                }
                len = textBottom - textTop;
                if (len < 0)
                    len += textBuffer.length;
                intBuf[0] = (byte) (len >> 24);
                intBuf[1] = (byte) (len >> 16);
                intBuf[2] = (byte) (len >> 8);
                intBuf[3] = (byte) len;
                printOutputStream.write(intBuf);
                intBuf[0] = (byte) (textPixelHeight >> 24);
                intBuf[1] = (byte) (textPixelHeight >> 16);
                intBuf[2] = (byte) (textPixelHeight >> 8);
                intBuf[3] = (byte) textPixelHeight;
                printOutputStream.write(intBuf);
                if (textTop <= textBottom)
                    printOutputStream.write(textBuffer, textTop, textBottom - textTop);
                else {
                    printOutputStream.write(textBuffer, textTop, textBuffer.length - textTop);
                    printOutputStream.write(textBuffer, 0, textBottom);
                }
            } catch (IOException e) {
                // Ignore
            } finally {
                if (printOutputStream != null)
                    try {
                        printOutputStream.close();
                    } catch (IOException e2) {}
            }
        }
    }

    private void doPrintAdv() {
        byte[] text = new byte[0];
        byte[] bits = new byte[162];
        shell_print(text, bits, 18, 0, 0, 143, 9);
    }

    
    ////////////////////////////////////////////////////////////////////
    ///// This section is where all the real 'shell' work is done. /////
    ////////////////////////////////////////////////////////////////////
    
    private boolean read_shell_state(IntHolder version) {
        try {
            int magic = state_read_int();
            if (magic != PLUS42_MAGIC() && magic != FREE42_MAGIC())
                return false;
            version.value = state_read_int();
            if (version.value < 0)
                return false;
            int shell_version = state_read_int();
            ShellSpool.printToGif = state_read_boolean();
            ShellSpool.printToGifFileName = state_read_string();
            ShellSpool.printToTxt = state_read_boolean();
            ShellSpool.printToTxtFileName = state_read_string();
            if (shell_version >= 1)
                ShellSpool.maxGifHeight = state_read_int();
            if (shell_version >= 2)
                skinName[0] = state_read_string();
            if (shell_version >= 3)
                externalSkinName[0] = state_read_string();
            if (shell_version >= 4) {
                skinName[1] = state_read_string();
                externalSkinName[1] = state_read_string();
                if (shell_version >= 17)
                    keyClicksLevel = state_read_int();
                else
                    keyClicksLevel = state_read_boolean() ? 3 : 0;
            } else {
                skinName[1] = skinName[0];
                externalSkinName[1] = externalSkinName[0];
                keyClicksLevel = 3;
            }
            if (shell_version >= 5)
                preferredOrientation = state_read_int();
            else
                preferredOrientation = ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED;
            if (shell_version >= 6) {
                skinSmoothing[0] = state_read_boolean();
                displaySmoothing[0] = state_read_boolean();
            }
            if (shell_version >= 7) {
                skinSmoothing[1] = state_read_boolean();
                displaySmoothing[1] = state_read_boolean();
            }
            if (shell_version >= 8)
                if (shell_version < 16) {
                    keyVibration = state_read_boolean() ? 12 : 0;
                } else {
                    keyVibration = state_read_int();
                    if (keyVibration > 16)
                        // The older 0, 50, 100, 150 scale
                        keyVibration = (int) (Math.log(keyVibration) / Math.log(2) * 2 + 0.5);
                }
            if (shell_version >= 9) {
                style = state_read_int();
                int maxStyle = PreferencesDialog.immersiveModeSupported ? 2 : 1;
                if (style > maxStyle)
                    style = maxStyle;
            } else
                style = 0;
            if (shell_version >= 10)
                alwaysRepaintFullDisplay = state_read_boolean();
            if (shell_version >= 11)
                alwaysOn = state_read_boolean();
            if (shell_version >= 13) {
                maintainSkinAspect[0] = state_read_boolean();
                maintainSkinAspect[1] = state_read_boolean();
            }
            if (shell_version >= 14)
                coreName = state_read_string();
            if (shell_version >= 15) {
                CoreSettings cs = new CoreSettings();
                getCoreSettings(cs);
                cs.matrix_singularmatrix = state_read_boolean();
                cs.matrix_outofrange = state_read_boolean();
                cs.auto_repeat = state_read_boolean();
                if (shell_version >= 18)
                    cs.allow_big_stack = state_read_boolean();
                putCoreSettings(cs);
            }
            init_shell_state(shell_version);
        } catch (IllegalArgumentException e) {
            return false;
        }
        return true;
    }
    
    private void init_shell_state(int shell_version) {
        CoreSettings cs = new CoreSettings();
        getCoreSettings(cs);
        switch (shell_version) {
        case -1:
            ShellSpool.printToGif = false;
            ShellSpool.printToGifFileName = "";
            ShellSpool.printToTxt = false;
            ShellSpool.printToTxtFileName = "";
            // fall through
        case 0:
            ShellSpool.maxGifHeight = 256;
            // fall through
        case 1:
            skinName[0] = "Standard";
            // fall through
        case 2:
            externalSkinName[0] = "";
            // fall through
        case 3:
            skinName[1] = "Landscape";
            externalSkinName[1] = "";
            keyClicksLevel = 3;
            // fall through
        case 4:
            preferredOrientation = ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED;
            // fall through
        case 5:
            skinSmoothing[0] = true;
            displaySmoothing[0] = false;
            // fall through
        case 6:
            skinSmoothing[1] = skinSmoothing[0];
            displaySmoothing[1] = displaySmoothing[0];
            // fall through
        case 7:
            keyVibration = 0;
            // fall through
        case 8:
            style = 0;
            // fall through
        case 9:
            alwaysRepaintFullDisplay = false;
            // fall through
        case 10:
            alwaysOn = false;
            // fall through
        case 11:
            // fall through
        case 12:
            maintainSkinAspect[0] = false;
            maintainSkinAspect[1] = false;
            // fall through
        case 13:
            coreName = "Untitled";
            // fall through
        case 14:
            cs.matrix_singularmatrix = false;
            cs.matrix_outofrange = false;
            cs.auto_repeat = true;
            // fall through
        case 15:
            // fall through
        case 16:
            // fall through
        case 17:
            cs.allow_big_stack = false;
            putCoreSettings(cs);
            // fall through
        case 18:
            // current version (SHELL_VERSION = 18),
            // so nothing to do here since everything
            // was initialized from the state file.
            ;
        }
    }

    private void write_shell_state() {
        try {
            state_write_int(PLUS42_MAGIC());
            state_write_int(27);
            state_write_int(SHELL_VERSION);
            state_write_boolean(ShellSpool.printToGif);
            state_write_string(ShellSpool.printToGifFileName);
            state_write_boolean(ShellSpool.printToTxt);
            state_write_string(ShellSpool.printToTxtFileName);
            state_write_int(ShellSpool.maxGifHeight);
            state_write_string(skinName[0]);
            state_write_string(externalSkinName[0]);
            state_write_string(skinName[1]);
            state_write_string(externalSkinName[1]);
            state_write_int(keyClicksLevel);
            state_write_int(preferredOrientation);
            state_write_boolean(skinSmoothing[0]);
            state_write_boolean(displaySmoothing[0]);
            state_write_boolean(skinSmoothing[1]);
            state_write_boolean(displaySmoothing[1]);
            state_write_int(keyVibration);
            state_write_int(style);
            state_write_boolean(alwaysRepaintFullDisplay);
            state_write_boolean(alwaysOn);
            state_write_boolean(maintainSkinAspect[0]);
            state_write_boolean(maintainSkinAspect[1]);
            state_write_string(coreName);
            CoreSettings cs = new CoreSettings();
            getCoreSettings(cs);
            state_write_boolean(cs.matrix_singularmatrix);
            state_write_boolean(cs.matrix_outofrange);
            state_write_boolean(cs.auto_repeat);
            state_write_boolean(cs.allow_big_stack);
        } catch (IllegalArgumentException e) {}
    }
    
    private byte[] int_buf = new byte[4];
    private int state_read_int() throws IllegalArgumentException {
        try {
            if (stateFileInputStream.read(int_buf) != 4)
                throw new IllegalArgumentException();
        } catch (IOException e) {
            throw new IllegalArgumentException();
        }
        return (int_buf[0] << 24) | ((int_buf[1] & 255) << 16) | ((int_buf[2] & 255) << 8) | (int_buf[3] & 255);
    }
    private void state_write_int(int i) throws IllegalArgumentException {
        int_buf[0] = (byte) (i >> 24);
        int_buf[1] = (byte) (i >> 16);
        int_buf[2] = (byte) (i >> 8);
        int_buf[3] = (byte) i;
        try {
            stateFileOutputStream.write(int_buf);
        } catch (IOException e) {
            throw new IllegalArgumentException();
        }
    }
    
    private byte[] boolean_buf = new byte[1];
    private boolean state_read_boolean() throws IllegalArgumentException {
        try {
            if (stateFileInputStream.read(boolean_buf) != 1)
                throw new IllegalArgumentException();
        } catch (IOException e) {
            throw new IllegalArgumentException();
        }
        return boolean_buf[0] != 0;
    }
    private void state_write_boolean(boolean b) throws IllegalArgumentException {
        boolean_buf[0] = (byte) (b ? 1 : 0);
        try {
            stateFileOutputStream.write(boolean_buf);
        } catch (IOException e) {
            throw new IllegalArgumentException();
        }
    }
    
    private String state_read_string() throws IllegalArgumentException {
        int length = state_read_int();
        byte[] buf = new byte[length];
        try {
            if (length > 0 && stateFileInputStream.read(buf) != length)
                throw new IllegalArgumentException();
            return new String(buf, "UTF-8");
        } catch (UnsupportedEncodingException e) {
            // Won't happen; UTF-8 is always supported.
            return null;
        } catch (IOException e) {
            throw new IllegalArgumentException();
        }
    }
    private void state_write_string(String s) throws IllegalArgumentException {
        byte[] buf;
        try {
            buf = s.getBytes("UTF-8");
            state_write_int(buf.length);
            stateFileOutputStream.write(buf);
        } catch (UnsupportedEncodingException e) {
            // Won't happen; UTF-8 is always supported.
            throw new IllegalArgumentException();
        } catch (IOException e) {
            throw new IllegalArgumentException();
        }
    }

    private void repeater() {
        cancelRepeaterAndTimeouts1And2();
        if (ckey == 0)
            return;
        int repeat = core_repeat();
        if (repeat != 0)
            mainHandler.postDelayed(repeaterCaller, repeat == 1 ? 200 : repeat == 2 ? 100 : 500);
        else
            mainHandler.postDelayed(timeout1Caller, 250);
    }

    private void timeout1() {
        cancelRepeaterAndTimeouts1And2();
        if (ckey != 0) {
            core_keytimeout1();
            mainHandler.postDelayed(timeout2Caller, 1750);
        }
    }

    private void timeout2() {
        cancelRepeaterAndTimeouts1And2();
        if (ckey != 0)
            core_keytimeout2();
    }

    private void timeout3() {
        cancelTimeout3();
        core_timeout3(true);
        // Resume program after PSE
        startRunner();
    }
    
    private void click() {
        if (keyClicksLevel > 0)
            playSound(keyClicksLevel + 10, 0);
        if (keyVibration > 0) {
            int ms = (int) (Math.pow(2, (keyVibration - 1) / 2.0) + 0.5);
            Vibrator v = (Vibrator) getSystemService(Context.VIBRATOR_SERVICE);
            v.vibrate(ms);
        }
    }
    
    
    public void playSound(int index, int duration) {
        soundPool.play(soundIds[index], 1f, 1f, 0, 0, 1f);
    }
    

    ////////////////////////////////////////////////////////////////////////////
    ///// Stubs for accessing the FREE42_MAGIC and FREE42_MAGIC_STR macros /////
    ////////////////////////////////////////////////////////////////////////////
    
    private static native int PLUS42_MAGIC();
    public static native String PLUS42_MAGIC_STR();
    private static native int FREE42_MAGIC();
    public static native String FREE42_MAGIC_STR();
    
    ///////////////////////////////////////////
    ///// Stubs for shell->core interface /////
    ///////////////////////////////////////////
    
    private native void nativeInit();
    
    private native void core_init(int read_state, int version, String state_file_name, int state_file_offset);
    private native void core_save_state(String state_file_name);
    private native void core_cleanup();
    private native void core_repaint_display();
    private native boolean core_menu();
    //private native boolean core_alpha_menu();
    //private native boolean core_hex_menu();
    private native boolean core_keydown(int key, BooleanHolder enqueued, IntHolder repeat, boolean immediate_return);
    private native boolean core_keydown_command(String cmd, BooleanHolder enqueued, IntHolder repeat, boolean immediate_return);
    private native int core_repeat();
    private native void core_keytimeout1();
    private native void core_keytimeout2();
    private native boolean core_timeout3(boolean repaint);
    private native boolean core_keyup();
    private native boolean core_powercycle();
    private native String[] core_list_programs();
    //private native int core_program_size(int prgm_index);
    private native void core_export_programs(int[] indexes, String raw_file_name);
    private native void core_import_programs(String raw_file_name);
    private native String core_copy();
    private native void core_paste(String s);
    private native void getCoreSettings(CoreSettings settings);
    private native void putCoreSettings(CoreSettings settings);
    private native void redisplay();
    private native void setAlwaysRepaintFullDisplay(boolean alwaysRepaint);
    private native void core_update_allow_big_stack();

    private static class CoreSettings {
        public boolean matrix_singularmatrix;
        public boolean matrix_outofrange;
        public boolean auto_repeat;
        public boolean allow_big_stack;
    }

    ///////////////////////////////////////////////////
    ///// Implementation of core->shell interface /////
    ///////////////////////////////////////////////////
    
    /**
     * shell_blitter()
     *
     * Callback invoked by the emulator core to cause the display, or some portion
     * of it, to be repainted.
     *
     * 'bits' is a pointer to a 1 bpp (monochrome) bitmap. The bits within a byte
     * are laid out with left corresponding to least significant, right
     * corresponding to most significant; this corresponds to the convention for
     * X11 images, but it is the reverse of the convention for MacOS and Windows.
     * The bytes are laid out sequentially, that is, bits[0] is at the top
     * left corner, bits[1] is to the right of bits[0], bits[2] is to the right of
     * bits[1], and so on; this corresponds to X11, MacOS, and Windows usage.
     * 'bytesperline' is the number of bytes per line of the bitmap; this means
     * that the bits just below bits[0] are at bits[bytesperline].
     * 'x', 'y', 'width', and 'height' define the part of the bitmap that needs to
     * be repainted. 'x' and 'y' are 0-based coordinates, with (0, 0) being the top
     * left corner of the bitmap, and x coordinates increasing to the right, and y
     * coordinates increasing downwards. 'width' and 'height' are the width and
     * height of the area to be repainted.
     */
    public void shell_blitter(byte[] bits, int bytesperline, int x, int y, int width, int height) {
        Rect inval = skin.display_blitter(bits, bytesperline, x, y, width, height);
        calcView.postInvalidateScaled(inval.left, inval.top, inval.right, inval.bottom);
    }

    /**
     * shell_beeper()
     * Callback invoked by the emulator core to play a sound.
     * The first parameter is the frequency in Hz; the second is the
     * duration in ms. The sound volume is up to the GUI to control.
     * Sound playback should be synchronous (the beeper function should
     * not return until the sound has finished), if possible.
     */
    public void shell_beeper(int frequency, int duration) {
        int sound_number = 10;
        for (int i = 0; i < 10; i++) {
            if (frequency <= cutoff_freqs[i]) {
                sound_number = i;
                break;
            }
        }
        playSound(sound_number, sound_number == 10 ? 125 : 250);
        try {
            Thread.sleep(sound_number == 10 ? 125 : 250);
        } catch (InterruptedException e) {}
    }

    private final int[] cutoff_freqs = { 164, 220, 243, 275, 293, 324, 366, 418, 438, 550 };
    
    private PrintAnnunciatorTurnerOffer pato = null;

    private class PrintAnnunciatorTurnerOffer extends Thread {
        public void run() {
            try {
                Thread.sleep(1000);
            } catch (InterruptedException e) {
                return;
            } finally {
                if (pato == this)
                    pato = null;
                else
                    return;
            }
            // Don't invalidate if the skin is null, which could happen
            // if we're in the process of switching between portrait and
            // landscape modes.
            SkinLayout currentSkin = skin;
            if (currentSkin != null) {
                Rect inval = currentSkin.update_annunciators(-1, -1, 0, -1, -1, -1, -1);
                if (inval != null)
                    calcView.postInvalidateScaled(inval.left, inval.top, inval.right, inval.bottom);
            }
        }
    }

    /**
     * shell_annunciators()
     * Callback invoked by the emulator core to change the state of the display
     * annunciators (up/down, shift, print, run, battery, (g)rad).
     * Every parameter can have values 0 (turn off), 1 (turn on), or -1 (leave
     * unchanged).
     * The battery annunciator is missing from the list; this is the only one of
     * the lot that the emulator core does not actually have any control over, and
     * so the shell is expected to handle that one by itself.
     */
    public void shell_annunciators(int updn, int shf, int prt, int run, int g, int rad) {
        boolean prt_off = false;
        if (prt != -1) {
            PrintAnnunciatorTurnerOffer p = pato;
            pato = null;
            if (p != null)
                p.interrupt();
            if (prt == 0) {
                prt = -1;
                prt_off = true;
            }
        }
        Rect inval = skin.update_annunciators(updn, shf, prt, run, -1, g, rad);
        if (inval != null)
            calcView.postInvalidateScaled(inval.left, inval.top, inval.right, inval.bottom);
        if (prt_off) {
            pato = new PrintAnnunciatorTurnerOffer();
            pato.start();
        }
    }
    
    /**
     * Callback to ask the shell to call core_timeout3() after the given number of
     * milliseconds. If there are keystroke events during that time, the timeout is
     * cancelled. (Pressing 'shift' does not cancel the timeout.)
     * This function supports the delay after SHOW, MEM, and shift-VARMENU.
     */
    public void shell_request_timeout3(int delay) {
        cancelTimeout3();
        mainHandler.postDelayed(timeout3Caller, delay);
        timeout3_active = true;
    }

    /**
     * shell_platform()
     * Callback to get the application version and platform.
     */
    public String shell_platform() {
        String version;
        try {
            version = getPackageManager().getPackageInfo(getPackageName(), 0).versionName;
        } catch (NameNotFoundException e) {
            version = "(Unknown)";
        }
        return version + " Android";
    }
    
    /**
     * shell_get_mem()
     * Callback to get the amount of free memory in bytes.
     */
    public int shell_get_mem() {
        long freeMem = Runtime.getRuntime().freeMemory();
        return freeMem > Integer.MAX_VALUE ? Integer.MAX_VALUE : (int) freeMem;
    }
    
    /**
     * shell_low_battery()
     * Callback to find out if the battery is low. Used to emulate flag 49 and the
     * battery annunciator.
     */
    public boolean shell_low_battery() {
        return low_battery;
    }
    
    /**
     * shell_powerdown()
     * Callback to tell the shell that the emulator wants to power down.
     * Only called in response to OFF (shift-EXIT or the OFF command); automatic
     * power-off is left to the OS and/or shell.
     */
    public void shell_powerdown() {
        quit_flag = true;
        finish();
    }
    
    private class AlwaysOnSetter implements Runnable {
        private boolean set;
        public AlwaysOnSetter(boolean set) {
            this.set = set;
        }
        public void run() {
            if (set)
                getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
            else
                getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        }
    }
    
    /**
     * shell_always_on()
     * Callback for setting and querying the shell's Continuous On status.
     */
    public boolean shell_always_on(int ao) {
        boolean ret = alwaysOn;
        if (ao != -1) {
            alwaysOn = ao != 0;
            runOnUiThread(new AlwaysOnSetter(alwaysOn));
        }
        return ret;
    }
    
    /**
     * shell_decimal_point()
     * Returns 0 if the host's locale uses comma as the decimal separator;
     * returns 1 if it uses dot or anything else.
     * Used to initialize flag 28 on hard reset.
     */
    public boolean shell_decimal_point() {
        DecimalFormat df = new DecimalFormat();
        DecimalFormatSymbols dfsym = df.getDecimalFormatSymbols();
        return dfsym.getDecimalSeparator() != ',';
    }
    
    /**
     * shell_date_format()
     * Returns 0 if the host's locale uses MDY date format;
     * returns 1 if it uses DMY;
     * returns 2 if it uses YMD.
     * If the host's date format doesn't match any of these three component
     * orders, returns 0.
     * Used to initialize flags 31 and 67 on hard reset.
     */
    public int shell_date_format() {
        Calendar cal = Calendar.getInstance();
        cal.set(3333, 10, 22);
        DateFormat fmt = DateFormat.getDateInstance(DateFormat.SHORT);
        String date = fmt.format(cal.getTime());
        int y = date.indexOf('3');
        int m = date.indexOf('1');
        int d = date.indexOf('2');
        if (d < m && m < y)
            return 1;
        else if (y < m && m < d)
            return 2;
        else
            return 0;
    }

    /* shell_clk24()
     * Returns 0 if the host's locale uses a 12-hour clock
     * returns 1 if it uses a 24-hour clock
     * Used to initialize CLK12/CLK24 mode on hard reset.
     */
    public boolean shell_clk24() {
        return android.text.format.DateFormat.is24HourFormat(this);
    }
    
    private OutputStream printTxtStream;
    private RandomAccessFile printGifFile;
    private int gif_lines;
    private int gif_seq = 0;
    
    /**
     * shell_print()
     * Printer emulation. The first 2 parameters are the plain text version of the
     * data to be printed; the remaining 6 parameters are the bitmap version. The
     * former is used for text-mode copying and for spooling to text files; the
     * latter is used for graphics-mode copying, spooling to image files, and
     * on-screen display.
     */
    public void shell_print(byte[] text, byte[] bits, int bytesperline,
                            int x, int y, int width, int height) {
        printPaperView.print(text, bits, bytesperline, x, y, width, height);

        if (ShellSpool.printToTxt) {
            try {
                if (printTxtStream == null)
                    if (new File(ShellSpool.printToTxtFileName).exists())
                        printTxtStream = new FileOutputStream(ShellSpool.printToTxtFileName, true);
                    else
                        printTxtStream = new FileOutputStream(ShellSpool.printToTxtFileName);
                if (text != null)
                    ShellSpool.shell_spool_txt(text, printTxtStream);
                else
                    ShellSpool.shell_spool_bitmap_to_txt(bits, bytesperline, x, y, width, height, printTxtStream);
            } catch (IOException e) {
                if (printTxtStream != null) {
                    try {
                        printTxtStream.close();
                    } catch (IOException e2) {}
                    printTxtStream = null;
                }
                ShellSpool.printToTxt = false;
                alert("An error occurred while printing to " + ShellSpool.printToTxtFileName + ": " + e.getMessage()
                        + "\nPrinting to text file disabled.");
            }
        }
        
        if (ShellSpool.printToGif) {
            if (printGifFile != null && gif_lines + height > ShellSpool.maxGifHeight) {
                try {
                    ShellSpool.shell_finish_gif(printGifFile);
                } catch (IOException e) {}
                try {
                    printGifFile.close();
                } catch (IOException e) {}
                printGifFile = null;
            }

            String name = null;
            if (printGifFile == null) {
                while (true) {
                    gif_seq = (gif_seq + 1) % 10000;
    
                    name = ShellSpool.printToGifFileName;
                    int len = name.length();

                    /* Strip ".gif" extension, if present */
                    if (len >= 4 && name.substring(len - 4).equals(".gif")) {
                        name = name.substring(0, len - 4);
                        len -= 4;
                    }
    
                    /* Strip ".[0-9]+", if present */
                    while (len > 0) {
                        char c = name.charAt(len - 1);
                        if (c >= '0' && c <= '9')
                            name = name.substring(0, --len);
                        else
                            break;
                    }
                    if (len > 0 && name.charAt(len - 1) == '.')
                        name = name.substring(0, --len);

                    String seq = "000" + gif_seq;
                    seq = seq.substring(seq.length() - 4);
                    name += "." + seq + ".gif";
    
                    if (!new File(name).exists())
                        break;
                }
            }

            try {
                if (name != null) {
                    printGifFile = new RandomAccessFile(name, "rw");
                    gif_lines = 0;
                    ShellSpool.shell_start_gif(printGifFile, ShellSpool.maxGifHeight);
                }
                ShellSpool.shell_spool_gif(bits, bytesperline, x, y, width, height, printGifFile);
                gif_lines += height;
            } catch (IOException e) {
                if (printGifFile != null) {
                    try {
                        printGifFile.close();
                    } catch (IOException e2) {}
                    printGifFile = null;
                }
                ShellSpool.printToGif = false;
                alert("An error occurred while printing to " + ShellSpool.printToGifFileName + ": " + e.getMessage()
                        + "\nPrinting to GIF file disabled.");
            }

            if (printGifFile != null && gif_lines + 9 > ShellSpool.maxGifHeight) {
                try {
                    ShellSpool.shell_finish_gif(printGifFile);
                } catch (IOException e) {}
                try {
                    printGifFile.close();
                } catch (IOException e) {}
                printGifFile = null;
            }
        }
    }

    /**
     * shell_message()
     * Shows a message box on behalf of the core.
     */
    public void shell_message(String message) {
        alert(message);
    }
    
    private boolean accel_inited, accel_exists;
    private double accel_x, accel_y, accel_z;
    
    public boolean shell_get_acceleration(DoubleHolder x, DoubleHolder y, DoubleHolder z) {
        if (!accel_inited) {
            accel_inited = true;
            SensorManager sm = (SensorManager) getSystemService(Context.SENSOR_SERVICE);
            Sensor s = sm.getDefaultSensor(Sensor.TYPE_ACCELEROMETER);
            if (s == null)
                return false;
            boolean success = sm.registerListener(new SensorEventListener() {
                        public void onAccuracyChanged(Sensor sensor, int accuracy) {
                            // Don't care
                        }
                        public void onSensorChanged(SensorEvent event) {
                            // Transform the measurements to conform to the iPhone
                            // conventions. The conversion factor used here is the
                            // 'standard gravity'.
                            accel_x = event.values[0] / -9.80665;
                            accel_y = event.values[1] / -9.80665;
                            accel_z = event.values[2] / -9.80665;
                        }
                    }, s, SensorManager.SENSOR_DELAY_NORMAL);
            if (!success)
                return false;
            accel_exists = true;
        }
        
        if (accel_exists) {
            x.value = accel_x;
            y.value = accel_y;
            z.value = accel_z;
            return true;
        } else {
            return false;
        }
    }

    private boolean locat_inited, locat_exists;
    private double locat_lat, locat_lon, locat_lat_lon_acc, locat_elev, locat_elev_acc;
    
    public boolean shell_get_location(DoubleHolder lat, DoubleHolder lon, DoubleHolder lat_lon_acc, DoubleHolder elev, DoubleHolder elev_acc) {
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION) != PackageManager.PERMISSION_GRANTED) {
            locat_inited = false;
            ActivityCompat.requestPermissions(this, new String[] { Manifest.permission.ACCESS_FINE_LOCATION }, MY_PERMISSIONS_REQUEST_ACCESS_FINE_LOCATION);
            return false;
        }
        if (!locat_inited) {
            locat_inited = true;
            LocationManager lm = (LocationManager) getSystemService(Context.LOCATION_SERVICE);
            Criteria cr = new Criteria();
            cr.setAccuracy(Criteria.ACCURACY_FINE);
            String provider = lm.getBestProvider(cr, true);
            if (provider == null) {
                locat_exists = false;
                return false;
            }
            LocationListener ll = new LocationListener() {
                public void onLocationChanged(Location location) {
                    // TODO: Verify units etc.
                    locat_lat = location.getLatitude();
                    locat_lon = location.getLongitude();
                    locat_lat_lon_acc = location.getAccuracy();
                    locat_elev = location.getAltitude();
                    locat_elev_acc = location.hasAltitude() ? locat_lat_lon_acc : -1;
                    geomagneticField = new GeomagneticField(
                            (float) locat_lat,
                            (float) locat_lon,
                            (float) locat_elev,
                            System.currentTimeMillis());
                }
                public void onProviderDisabled(String provider) {
                    // Ignore
                }
                public void onProviderEnabled(String provider) {
                    // Ignore
                }
                public void onStatusChanged(String provider, int status,
                        Bundle extras) {
                    // Ignore
                }
            };
            try {
                lm.requestLocationUpdates(provider, 5000, 1, ll, Looper.getMainLooper());
            } catch (IllegalArgumentException e) {
                return false;
            } catch (SecurityException e) {
                return false;
            }
            locat_exists = true;
        }
        
        if (locat_exists) {
            lat.value = locat_lat;
            lon.value = locat_lon;
            lat_lon_acc.value = locat_lat_lon_acc;
            elev.value = locat_elev;
            elev_acc.value = locat_elev_acc;
            return true;
        } else
            return false;
    }
    
    private boolean heading_inited, heading_exists;
    private double heading_mag, heading_true, heading_acc;
    private float[] gravity = new float[3];
    private float[] geomagnetic = new float[3];
    private float[] rotation = new float[9];
    private float[] orient = new float[3];
    private LowPassFilter gravityFilter = new LowPassFilter();
    private LowPassFilter geomagneticFilter = new LowPassFilter();
    private GeomagneticField geomagneticField = null;

    private static class LowPassFilter {
        private float[] f = new float[3];
        public float[] filter(float[] in) {
            f[0] = 0.75f * f[0] + 0.25f * in[0];
            f[1] = 0.75f * f[1] + 0.25f * in[1];
            f[2] = 0.75f * f[2] + 0.25f * in[2];
            return f;
        }
    }
    
    public boolean shell_get_heading(DoubleHolder mag_heading, DoubleHolder true_heading, DoubleHolder acc_heading, DoubleHolder x, DoubleHolder y, DoubleHolder z) {
        if (!heading_inited) {
            heading_inited = true;
            SensorManager sm = (SensorManager) getSystemService(Context.SENSOR_SERVICE);
            Sensor s1 = sm.getDefaultSensor(Sensor.TYPE_ACCELEROMETER);
            Sensor s2 = sm.getDefaultSensor(Sensor.TYPE_MAGNETIC_FIELD);
            SensorEventListener listener = new SensorEventListener() {
                        private float[] smoothed = new float[3];
                        public void onAccuracyChanged(Sensor sensor, int accuracy) {
                            // Don't care
                        }
                        public void onSensorChanged(SensorEvent event) {
                            if (event.sensor.getType() == Sensor.TYPE_ACCELEROMETER) {
                                smoothed = gravityFilter.filter(event.values);
                                gravity[0] = smoothed[0];
                                gravity[1] = smoothed[1];
                                gravity[2] = smoothed[2];
                            } else if (event.sensor.getType() == Sensor.TYPE_MAGNETIC_FIELD) {
                                smoothed = geomagneticFilter.filter(event.values);
                                geomagnetic[0] = smoothed[0];
                                geomagnetic[1] = smoothed[1];
                                geomagnetic[2] = smoothed[2];
                            }

                            // get rotation matrix to get gravity and magnetic data
                            SensorManager.getRotationMatrix(rotation, null, gravity, geomagnetic);
                            // get bearing to target
                            SensorManager.getOrientation(rotation, orient);
                            // east degrees of true North
                            heading_mag = orient[0];
                            // convert from radians to degrees
                            heading_mag = Math.toDegrees(heading_mag);

                            // TODO: fix difference between true North and magnetic North
                            if (geomagneticField == null) {
                                heading_true = 0;
                                heading_acc = -1;
                            } else {
                                heading_true = heading_mag + geomagneticField.getDeclination();
                                if (heading_true < 0)
                                    heading_true += 360;
                                else if (heading_true >= 360)
                                    heading_true -= 360;
                            }
                        }
                    };
            sm.registerListener(listener, s1, SensorManager.SENSOR_DELAY_UI);
            sm.registerListener(listener, s2, SensorManager.SENSOR_DELAY_UI);
            heading_exists = true;
        }
        
        if (heading_exists) {
            mag_heading.value = heading_mag;
            true_heading.value = heading_true;
            acc_heading.value = heading_acc;
            x.value = geomagnetic[0];
            y.value = geomagnetic[1];
            z.value = geomagnetic[2];
            return true;
        } else {
            return false;
        }
    }
    
    public void shell_log(String s) {
        System.err.print(s);
    }
    
    public static boolean checkStorageAccess() {
        return instance.checkStorageAccess2();
    }
    
    private boolean checkStorageAccess2() {
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.WRITE_EXTERNAL_STORAGE) == PackageManager.PERMISSION_GRANTED) {
            getExternalFilesDir(null).mkdirs();
            return true;
        }
        ActivityCompat.requestPermissions(this, new String[] { Manifest.permission.WRITE_EXTERNAL_STORAGE }, MY_PERMISSIONS_REQUEST_ACCESS_FINE_LOCATION);
        return false;
    }
}
