package android.os;

import java.io.FileDescriptor;

public interface IBinder {
	int FIRST_CALL_TRANSACTION = 0x00000001;
	int LAST_CALL_TRANSACTION = 0x00ffffff;
	int PING_TRANSACTION = ('_' << 24) | ('P' << 16) | ('N' << 8) | 'G';
	int DUMP_TRANSACTION = ('_' << 24) | ('D' << 16) | ('M' << 8) | 'P';
	int INTERFACE_TRANSACTION = ('_' << 24) | ('N' << 16) | ('T' << 8) | 'F';
	int FLAG_ONEWAY = 0x00000001;

	public interface DeathRecipient {
		default void binderDied() {}
		default void binderDied(IBinder who) { binderDied(); }
	}

	public IInterface queryLocalInterface(String descriptor);

	public boolean transact(int code, Parcel data, Parcel reply, int flags) throws RemoteException;

	public String getInterfaceDescriptor();

	// every binder lives in this one process: none dies before it
	default void linkToDeath(DeathRecipient recipient, int flags) throws RemoteException {}

	default boolean unlinkToDeath(DeathRecipient recipient, int flags) { return true; }

	default boolean isBinderAlive() { return true; }

	default boolean pingBinder() { return true; }

	default void dump(FileDescriptor fd, String[] args) throws RemoteException {}

	// GEN member:android.os.IBinder:public static int getSuggestedMaxIpcSizeBytes();
	static int getSuggestedMaxIpcSizeBytes() { return 0; }
}
