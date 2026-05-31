using System.Text;
using Xunit;

namespace Sovereign.Tests;

public class CarmillaEngineTests
{
    [Fact]
    public void EncryptDecrypt_Roundtrip_Succeeds()
    {
        var plain = Encoding.UTF8.GetBytes("Hello, RawrXD! This is a secret message.");
        var passphrase = "super-secret-passphrase-123";

        var encrypted = CarmillaEngine.Encrypt(plain, passphrase);
        Assert.NotNull(encrypted);
        Assert.True(encrypted.Length > plain.Length);

        var decrypted = CarmillaEngine.Decrypt(encrypted, passphrase);
        Assert.Equal(plain, decrypted);
    }

    [Theory]
    [InlineData("short")]
    [InlineData("")]
    [InlineData("The quick brown fox jumps over the lazy dog. 1234567890 !@#$%^&*()")]
    public void EncryptDecrypt_VariousLengths_Succeeds(string text)
    {
        var plain = Encoding.UTF8.GetBytes(text);
        var passphrase = "key";

        var encrypted = CarmillaEngine.Encrypt(plain, passphrase);
        var decrypted = CarmillaEngine.Decrypt(encrypted, passphrase);

        Assert.Equal(plain, decrypted);
    }

    [Fact]
    public void Decrypt_WithWrongPassphrase_Throws()
    {
        var plain = Encoding.UTF8.GetBytes("secret data");
        var encrypted = CarmillaEngine.Encrypt(plain, "correct-horse-battery-staple");

        Assert.Throws<System.Security.Cryptography.AuthenticationTagMismatchException>(() =>
            CarmillaEngine.Decrypt(encrypted, "wrong-passphrase"));
    }

    [Fact]
    public void Encrypt_ProducesDifferentCiphertexts()
    {
        var plain = Encoding.UTF8.GetBytes("deterministic-plaintext");
        var passphrase = "same-key";

        var enc1 = CarmillaEngine.Encrypt(plain, passphrase);
        var enc2 = CarmillaEngine.Encrypt(plain, passphrase);

        Assert.NotEqual(enc1, enc2); // nonce should be random
    }
}
