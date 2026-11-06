/*
 * Copyright (C) 2006 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package android.os;

/**
 * Reference to a Handler, which others can use to send messages to it.
 * This allows for the implementation of message-based communication across
 * processes, by creating a Messenger pointing to a Handler in one process,
 * and handing that Messenger to another process.
 */
public final class Messenger implements Parcelable {
	private final IMessenger mTarget;

	/**
	 * Create a new Messenger pointing to the given Handler.  Any Message
	 * objects sent through this Messenger will appear in the Handler as if
	 * {@link Handler#sendMessage(Message) Handler.sendMessage(Message)} had
	 * been called directly.
	 *
	 * @param target The Handler that will receive sent messages.
	 */
	public Messenger(Handler target) {
		mTarget = target.getIMessenger();
	}

	/**
	 * Send a Message to this Messenger's Handler.
	 *
	 * @param message The Message to send.  Usually retrieved through
	 * {@link Message#obtain() Message.obtain()}.
	 *
	 * @throws RemoteException Throws DeadObjectException if the target
	 * Handler no longer exists.
	 */
	public void send(Message message) throws RemoteException {
		mTarget.send(message);
	}

	/**
	 * Retrieve the IBinder that this Messenger is using to communicate with
	 * its associated Handler.
	 *
	 * @return Returns the IBinder backing this Messenger.
	 */
	public IBinder getBinder() {
		// return mTarget.asBinder();
		return new IBinder() {
			@Override
			public String getInterfaceDescriptor() {
				return "android.os.IMessenger";
			}
			@Override
			public IInterface queryLocalInterface(String descriptor) {
				return null;
			}
			@Override
			public boolean transact(int code, Parcel data, Parcel reply, int flags) throws RemoteException {
				mTarget.send(Message.CREATOR.createFromParcel(data));
				// a oneway send brings no reply parcel
				if (reply != null) {
					reply.writeNoException();
					reply.setDataPosition(0);
				}
				return true;
			}
		};
	}

	/**
	 * Comparison operator on two Messenger objects, such that true
	 * is returned then they both point to the same Handler.
	 */
	public boolean equals(Object otherObj) {
		/*if (otherObj == null) {
		    return false;
		}
		try {
		    return mTarget.asBinder().equals(((Messenger)otherObj)
			    .mTarget.asBinder());
		} catch (ClassCastException e) {
		}*/
		return false;
	}

	public int hashCode() {
		return 0; // mTarget.asBinder().hashCode();
	}

	public int describeContents() {
		return 0;
	}

	/**
	 * Create a Messenger from a raw IBinder, which had previously been
	 * retrieved with {@link #getBinder}.
	 *
	 * @param target The IBinder this Messenger should communicate with.
	 */
	public Messenger(IBinder target) {
		mTarget = new IMessenger() {
			@Override
			public void send(Message msg) throws RemoteException {
				Parcel data = Parcel.obtain();
				Parcel reply = Parcel.obtain();
				msg.writeToParcel(data, 0);
				data.setDataPosition(0);
				target.transact(1, data, reply, 0);
				reply.readException();
				data.recycle();
				reply.recycle();
			}
		};
	}

	public static void writeMessengerOrNullToParcel(Messenger messenger, Parcel dest) {
		dest.writeStrongBinder(messenger != null ? messenger.getBinder() : null);
	}

	public static Messenger readMessengerOrNullFromParcel(Parcel source) {
		IBinder binder = source.readStrongBinder();
		return binder != null ? new Messenger(binder) : null;
	}
}
