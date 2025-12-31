Vector file format (codec):

  <name> <raw_hex> <expect_ok 0|1> <expect_payload_dec> <expect_tail 0|1> <expect_err_mask_hex>

Notes:
- raw_hex is the packed DATA bus value (DATA[11:0] for default config).
- expect_err_mask_hex is a *minimum* mask; the test requires (err_flags & mask) == mask.
- Run tests from the repository root so relative paths resolve:
    tests/vectors/codec_valid.txt
    tests/vectors/codec_invalid.txt
