import java.security.*;
import javax.crypto.*;
import javax.crypto.spec.*;
import java.util.*;

/* DoD: java.security / javax.crypto — MessageDigest(SHA-256/MD5), Mac(HMAC),
 * Cipher(AES/GCM, 固定密钥避免熵阻塞), SecureRandom(SHA1PRNG+setSeed 确定性,
 * 避开 starry 熵阻塞). 纯计算, 验 JCE provider 完整。 */
public class CryptoTest {
    static int ok = 0, fail = 0;
    static void check(boolean c, String n) { if (c) ok++; else { fail++; System.out.println("FAIL " + n); } }
    static String hex(byte[] b) { StringBuilder s = new StringBuilder(); for (byte x : b) s.append(String.format("%02x", x)); return s.toString(); }

    public static void main(String[] args) throws Exception {
        // MessageDigest SHA-256 (known vector for "abc")
        byte[] sha = MessageDigest.getInstance("SHA-256").digest("abc".getBytes());
        check(hex(sha).equals("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"), "sha256");
        byte[] md5 = MessageDigest.getInstance("MD5").digest("abc".getBytes());
        check(hex(md5).equals("900150983cd24fb0d6963f7d28e17f72"), "md5");

        // HMAC-SHA256
        Mac mac = Mac.getInstance("HmacSHA256");
        mac.init(new SecretKeySpec("key".getBytes(), "HmacSHA256"));
        check(hex(mac.doFinal("data".getBytes())).length() == 64, "hmac-sha256");

        // AES/GCM round-trip (fixed key+iv, no SecureRandom)
        byte[] key = new byte[16]; for (int i = 0; i < 16; i++) key[i] = (byte) i;
        byte[] iv = new byte[12]; for (int i = 0; i < 12; i++) iv[i] = (byte) (i + 1);
        SecretKeySpec sk = new SecretKeySpec(key, "AES");
        GCMParameterSpec gcm = new GCMParameterSpec(128, iv);
        Cipher enc = Cipher.getInstance("AES/GCM/NoPadding"); enc.init(Cipher.ENCRYPT_MODE, sk, gcm);
        byte[] ct = enc.doFinal("secret message".getBytes());
        Cipher dec = Cipher.getInstance("AES/GCM/NoPadding"); dec.init(Cipher.DECRYPT_MODE, sk, gcm);
        check(new String(dec.doFinal(ct)).equals("secret message"), "aes-gcm-roundtrip");

        // AES/CBC round-trip
        Cipher cbcE = Cipher.getInstance("AES/CBC/PKCS5Padding");
        cbcE.init(Cipher.ENCRYPT_MODE, sk, new IvParameterSpec(new byte[16]));
        byte[] cbc = cbcE.doFinal("block cipher test".getBytes());
        Cipher cbcD = Cipher.getInstance("AES/CBC/PKCS5Padding");
        cbcD.init(Cipher.DECRYPT_MODE, sk, new IvParameterSpec(new byte[16]));
        check(new String(cbcD.doFinal(cbc)).equals("block cipher test"), "aes-cbc-roundtrip");

        // SecureRandom — SHA1PRNG with explicit seed (deterministic, no /dev/random block)
        SecureRandom sr = SecureRandom.getInstance("SHA1PRNG");
        sr.setSeed(12345L);
        byte[] rnd = new byte[16]; sr.nextBytes(rnd);
        check(Arrays.equals(rnd, new byte[16]) == false, "securerandom-sha1prng");

        System.out.println("CRYPTO_RESULT ok=" + ok + " fail=" + fail);
        if (fail == 0) System.out.println("CRYPTO_DONE");
    }
}
