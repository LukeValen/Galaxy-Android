package com.openport.galaxyandroid;

import android.app.Activity;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.Looper;
import android.provider.OpenableColumns;
import android.provider.Settings;
import android.text.method.ScrollingMovementMethod;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.ProgressBar;
import android.widget.TextView;
import android.widget.Toast;

import java.io.File;

public class MainActivity extends Activity {
    private static final String TAG = "GalaxyAndroid";
    private static final int REQ_PICK_DISC = 1001;

    private TextView txtHeaderTitle;
    private TextView txtSourcePath;
    private CheckBox chkIncludeMovies;
    private Button btnSelectDisc;
    private Button btnAutoDetect;
    private Button btnStartCooking;
    private Button btnLaunchGame;
    private ProgressBar progressBar;
    private TextView txtProgressStatus;
    private TextView txtLogOutput;

    private String mSelectedSourcePath = "";
    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private boolean mIsCooking = false;

    public static boolean isGameDataReady() {
        File dataDir = new File("/sdcard/GalaxyData");
        if (!dataDir.exists()) return false;
        File fst = new File(dataDir, "sys/fst.bin");
        File files = new File(dataDir, "files");
        File coin = new File(dataDir, "files/ObjectData/Coin.arc");
        return fst.exists() && fst.isFile() && files.exists() && files.isDirectory() && coin.exists();
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        boolean forceSetup = getIntent().getBooleanExtra("force_setup", false);
        // Instant direct launch if game is already installed
        if (!forceSetup && isGameDataReady()) {
            launchGameDirectly();
            return;
        }

        setContentView(R.layout.activity_main);

        txtHeaderTitle = findViewById(R.id.txtHeaderTitle);
        txtSourcePath = findViewById(R.id.txtSourcePath);
        chkIncludeMovies = findViewById(R.id.chkIncludeMovies);
        btnSelectDisc = findViewById(R.id.btnSelectDisc);
        btnAutoDetect = findViewById(R.id.btnAutoDetect);
        btnStartCooking = findViewById(R.id.btnStartCooking);
        btnLaunchGame = findViewById(R.id.btnLaunchGame);
        progressBar = findViewById(R.id.progressBar);
        txtProgressStatus = findViewById(R.id.txtProgressStatus);
        txtLogOutput = findViewById(R.id.txtLogOutput);

        txtLogOutput.setMovementMethod(new ScrollingMovementMethod());

        checkStoragePermissions();
        autoDetectSource();

        btnSelectDisc.setOnClickListener(v -> openFilePicker());
        btnAutoDetect.setOnClickListener(v -> autoDetectSource());

        btnStartCooking.setOnClickListener(v -> {
            if (mSelectedSourcePath.isEmpty()) {
                Toast.makeText(this, "Please select a game disc image (.rvz / .iso / .wbfs)", Toast.LENGTH_SHORT).show();
                return;
            }
            startCookingProcess();
        });

        btnLaunchGame.setOnClickListener(v -> launchGameDirectly());
    }

    private void launchGameDirectly() {
        Intent intent = new Intent(this, GameActivity.class);
        startActivity(intent);
        finish();
    }

    private void checkStoragePermissions() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            if (!Environment.isExternalStorageManager()) {
                appendLog("[Permission] Requesting storage access for game data...");
                Intent intent = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION);
                intent.setData(Uri.parse("package:" + getPackageName()));
                try {
                    startActivity(intent);
                } catch (Exception e) {
                    Intent generic = new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION);
                    startActivity(generic);
                }
            }
        }
    }

    private void autoDetectSource() {
        appendLog("[Scan] Searching for game dumps and extracted folders...");

        // 1. Check if /sdcard/GalaxyData already exists
        File galaxyDataDir = new File(Environment.getExternalStorageDirectory(), "GalaxyData");
        if (galaxyDataDir.exists()) {
            if (isGameDataReady()) {
                appendLog("[Scan] Game data is ready in " + galaxyDataDir.getAbsolutePath() + "!");
                mHandler.post(() -> {
                    btnLaunchGame.setVisibility(View.VISIBLE);
                    txtProgressStatus.setText("Game data ready! Tap Launch to play.");
                });
            }
            File[] gFiles = galaxyDataDir.listFiles();
            if (gFiles != null) {
                for (File f : gFiles) {
                    String name = f.getName().toLowerCase();
                    if (name.endsWith(".rvz") || name.endsWith(".wbfs") || name.endsWith(".iso")) {
                        selectSource(f.getAbsolutePath(), "Detected disc in GalaxyData: " + f.getName());
                        return;
                    }
                }
            }
        }

        // 2. Scan Downloads folder
        File downloadDir = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS);
        if (downloadDir != null && downloadDir.exists()) {
            File[] files = downloadDir.listFiles();
            if (files != null) {
                for (File f : files) {
                    String name = f.getName().toLowerCase();
                    if (name.endsWith(".rvz") || name.endsWith(".wbfs") || name.endsWith(".iso")) {
                        selectSource(f.getAbsolutePath(), "Detected disc in Downloads: " + f.getName());
                        return;
                    }
                }
                for (File f : files) {
                    if (f.isDirectory()) {
                        File testFiles = new File(f, "files");
                        File testData = new File(f, "DATA");
                        if (testFiles.exists() || testData.exists()) {
                            selectSource(f.getAbsolutePath(), "Detected extracted folder: " + f.getName());
                            return;
                        }
                    }
                }
            }
        }

        appendLog("[Scan] No disc image auto-detected. Use 'Choose Disc Image' button.");
    }

    private void selectSource(String path, String description) {
        mSelectedSourcePath = path;
        appendLog("[Selected] " + description);
        mHandler.post(() -> {
            txtSourcePath.setText(path);
            txtProgressStatus.setText("Disc image selected. Ready to install.");
            btnStartCooking.setEnabled(true);
        });
    }

    private void openFilePicker() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");
        startActivityForResult(intent, REQ_PICK_DISC);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (resultCode != RESULT_OK || data == null || data.getData() == null) return;

        Uri uri = data.getData();
        if (requestCode == REQ_PICK_DISC) {
            String path = getPathFromUri(uri);
            if (path != null) {
                selectSource(path, "Selected disc: " + new File(path).getName());
            } else {
                selectSource(uri.toString(), "Selected URI: " + uri.toString());
            }
        }
    }

    private String getPathFromUri(Uri uri) {
        if ("file".equalsIgnoreCase(uri.getScheme())) {
            return uri.getPath();
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
            String path = uri.getPath();
            if (path != null && path.contains(":")) {
                String[] parts = path.split(":");
                if (parts.length > 1) {
                    File f = new File(Environment.getExternalStorageDirectory(), parts[1]);
                    if (f.exists()) return f.getAbsolutePath();
                }
            }
        }
        return null;
    }

    private void startCookingProcess() {
        if (mIsCooking) return;
        mIsCooking = true;

        btnStartCooking.setEnabled(false);
        btnSelectDisc.setEnabled(false);
        btnAutoDetect.setEnabled(false);
        progressBar.setVisibility(View.VISIBLE);
        progressBar.setIndeterminate(true);
        txtProgressStatus.setText("Preparing game installation...");

        new Thread(() -> {
            try {
                File outputDir = new File(Environment.getExternalStorageDirectory(), "GalaxyData");
                if (!outputDir.exists()) outputDir.mkdirs();

                boolean includeMovies = chkIncludeMovies.isChecked();
                File sourceFile = new File(mSelectedSourcePath);
                String sourceLower = mSelectedSourcePath.toLowerCase();

                File extractionDir = outputDir;
                File tempExtractionDir = null;

                // Step 1: If source is disc image (.rvz, .iso, .wbfs), extract with native nodtool
                if (sourceLower.endsWith(".rvz") || sourceLower.endsWith(".wbfs") || sourceLower.endsWith(".iso")) {
                    tempExtractionDir = new File(getFilesDir(), "temp_extracted");
                    CookerEngine.deleteRecursive(tempExtractionDir);
                    tempExtractionDir.mkdirs();

                    mHandler.post(() -> txtProgressStatus.setText("Extracting disc partition..."));
                    boolean extractSuccess = CookerEngine.extractWithNodtool(
                        this, mSelectedSourcePath, tempExtractionDir.getAbsolutePath(), includeMovies,
                        new CookerEngine.CookerCallback() {
                            @Override public void onLog(String line) { appendLog(line); }
                            @Override public void onProgress(int percent, String message) {
                                mHandler.post(() -> {
                                    if (message != null) txtProgressStatus.setText(message);
                                });
                            }
                            @Override public void onFinished(boolean success, String message) {}
                        }
                    );

                    if (!extractSuccess) {
                        mHandler.post(() -> onCookingCompleted(false, "Native disc extraction failed."));
                        return;
                    }
                    extractionDir = tempExtractionDir;
                }

                // Step 2: Prepare converter environment and python runtime
                mHandler.post(() -> txtProgressStatus.setText("Setting up converter engine..."));
                CookerEngine.prepareAssets(this, new CookerEngine.CookerCallback() {
                    @Override public void onLog(String line) { appendLog(line); }
                    @Override public void onProgress(int percent, String message) {}
                    @Override public void onFinished(boolean success, String message) {}
                });

                File filesDir = getFilesDir();
                File pythonHome = new File(filesDir, "pythonhome");
                File scriptsDir = new File(filesDir, "scripts");

                // Step 3: Run asset converter into GalaxyData
                mHandler.post(() -> txtProgressStatus.setText("Converting assets to little-endian ARM64..."));
                int ret = CookerEngine.runCooker(
                    pythonHome.getAbsolutePath(),
                    scriptsDir.getAbsolutePath(),
                    extractionDir.getAbsolutePath(),
                    outputDir.getAbsolutePath(),
                    includeMovies,
                    new CookerEngine.CookerCallback() {
                        @Override public void onLog(String line) { appendLog(line); }
                        @Override public void onProgress(int percent, String message) {
                            mHandler.post(() -> {
                                if (percent >= 0) {
                                    progressBar.setIndeterminate(false);
                                    progressBar.setProgress(percent);
                                }
                                if (message != null) txtProgressStatus.setText(message);
                            });
                        }
                        @Override public void onFinished(boolean success, String message) {
                            mHandler.post(() -> onCookingCompleted(success, message));
                        }
                    }
                );

                if (ret != 0) {
                    mHandler.post(() -> onCookingCompleted(false, "Converter returned code: " + ret));
                } else {
                    if (tempExtractionDir != null && tempExtractionDir.exists()) {
                        appendLog("[Clean] Freeing temporary extraction cache...");
                        CookerEngine.deleteRecursive(tempExtractionDir);
                    }
                    mHandler.post(() -> onCookingCompleted(true, "Installation complete!"));
                }

            } catch (Exception e) {
                Log.e(TAG, "Cooking failed with exception", e);
                appendLog("[ERROR] " + e.getMessage());
                mHandler.post(() -> onCookingCompleted(false, e.getMessage()));
            }
        }).start();
    }

    private void onCookingCompleted(boolean success, String message) {
        mIsCooking = false;
        progressBar.setVisibility(View.GONE);
        btnStartCooking.setEnabled(true);
        btnSelectDisc.setEnabled(true);
        btnAutoDetect.setEnabled(true);

        if (success) {
            txtProgressStatus.setText(R.string.status_done);
            appendLog("[SUCCESS] " + (message != null ? message : "Conversion complete!"));
            btnLaunchGame.setVisibility(View.VISIBLE);
            Toast.makeText(this, "Game ready! Launching game now...", Toast.LENGTH_SHORT).show();
            // Automatically launch into game!
            mHandler.postDelayed(this::launchGameDirectly, 1000);
        } else {
            txtProgressStatus.setText(R.string.status_error);
            appendLog("[FAILURE] " + (message != null ? message : "Error during installation."));
            Toast.makeText(this, "Installation failed. Check log.", Toast.LENGTH_LONG).show();
        }
    }

    private void appendLog(String line) {
        mHandler.post(() -> {
            txtLogOutput.append(line + "\n");
            final int scrollAmount = txtLogOutput.getLayout() != null ?
                    txtLogOutput.getLayout().getLineTop(txtLogOutput.getLineCount()) - txtLogOutput.getHeight() : 0;
            if (scrollAmount > 0) {
                txtLogOutput.scrollTo(0, scrollAmount);
            }
        });
    }
}
