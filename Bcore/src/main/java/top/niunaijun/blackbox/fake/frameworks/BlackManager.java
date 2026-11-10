package top.niunaijun.blackbox.fake.frameworks;

import android.os.IBinder;
import android.os.IInterface;
import android.util.Log;

import java.lang.reflect.ParameterizedType;
import java.util.concurrent.atomic.AtomicBoolean;

import top.niunaijun.blackbox.BlackBoxCore;
import top.niunaijun.blackbox.utils.Reflector;

public abstract class BlackManager<Service extends IInterface> {
    public static final String TAG = "BlackManager";

    private final Object mLock = new Object();
    private volatile Service mService;
    private final AtomicBoolean mServiceCreationFailed = new AtomicBoolean(false);
    private long mLastRetryTime = 0;
    private static final long RETRY_TIMEOUT_MS = 200;

    protected abstract String getServiceName();

    public Service getService() {
        Service service = mService;
        if (service != null && service.asBinder() != null && service.asBinder().isBinderAlive() && service.asBinder().pingBinder()) {
            return service;
        }

        synchronized (mLock) {
            service = mService;
            if (service != null && service.asBinder() != null && service.asBinder().isBinderAlive() && service.asBinder().pingBinder()) {
                return service;
            }

            long currentTime = System.currentTimeMillis();
            if (mServiceCreationFailed.get()) {
                if (currentTime - mLastRetryTime < RETRY_TIMEOUT_MS) {
                    Log.d(TAG, "Skipping service creation for " + getServiceName() + " due to recent failure");
                    return mService;
                }
                mServiceCreationFailed.set(false);
            }

            try {
                IBinder binder = BlackBoxCore.get().getService(getServiceName());
                if (binder == null || !binder.isBinderAlive()) {
                    Log.w(TAG, "Failed to get alive binder for service: " + getServiceName());
                    markServiceCreationFailed();
                    return null;
                }

                String stubClassName = getTClass().getName() + "$Stub";
                Log.d(TAG, "Creating service for: " + stubClassName);

                mService = Reflector.on(stubClassName).method("asInterface", IBinder.class)
                        .call(binder);

                if (mService != null && mService.asBinder() != null && mService.asBinder().isBinderAlive()) {
                    final Service serviceRef = mService;
                    try {
                        serviceRef.asBinder().linkToDeath(new IBinder.DeathRecipient() {
                            @Override
                            public void binderDied() {
                                try {
                                    if (serviceRef != null && serviceRef.asBinder() != null) {
                                        serviceRef.asBinder().unlinkToDeath(this, 0);
                                    }
                                } catch (Exception e) {
                                    Log.w(TAG, "Error unlinking death recipient for " + getServiceName(), e);
                                }
                                synchronized (mLock) {
                                    if (mService == serviceRef) {
                                        mService = null;
                                    }
                                }
                                Log.w(TAG, "Service died: " + getServiceName());
                            }
                        }, 0);
                    } catch (Exception e) {
                        Log.w(TAG, "Error linking death recipient for " + getServiceName(), e);
                    }

                    Log.d(TAG, "Successfully created service: " + getServiceName());
                    mServiceCreationFailed.set(false);
                } else {
                    Log.w(TAG, "Failed to create service instance for: " + getServiceName());
                    mService = null;
                    markServiceCreationFailed();
                }

                return mService;
            } catch (Throwable e) {
                Log.e(TAG, "Error creating service for " + getServiceName(), e);
                mService = null;
                markServiceCreationFailed();
                return null;
            }
        }
    }

    private void markServiceCreationFailed() {
        mServiceCreationFailed.set(true);
        mLastRetryTime = System.currentTimeMillis();
    }

    public void clearServiceCache() {
        synchronized (mLock) {
            mService = null;
        }
        Log.d(TAG, "Cleared service cache for " + getServiceName());
    }

    public boolean isServiceHealthy() {
        Service service = mService;
        if (service == null || service.asBinder() == null) {
            return false;
        }
        try {
            return service.asBinder().pingBinder() && service.asBinder().isBinderAlive();
        } catch (Exception e) {
            Log.w(TAG, "Service health check failed for " + getServiceName(), e);
            return false;
        }
    }

    @SuppressWarnings("unchecked")
    private Class<Service> getTClass() {
        return (Class<Service>) ((ParameterizedType) getClass().getGenericSuperclass()).getActualTypeArguments()[0];
    }
}
