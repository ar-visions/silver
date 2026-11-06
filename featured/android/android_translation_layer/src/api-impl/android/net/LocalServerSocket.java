package android.net;

import java.io.IOException;

public class LocalServerSocket implements java.io.Closeable {

	public LocalServerSocket(String name) {}

	public LocalSocket accept() throws IOException {
		throw new IOException("LocalServerSocket not yet implemented");
	}
	public void close() throws java.io.IOException {}
}
