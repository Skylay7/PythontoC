def test_inference(base_score, multiplier, username, unused_param):
    # base_score interacts only with integers, should stay STYPE_INT
    # base_score interacts only with integers, should stay STYPE_INT
    # base_score interacts only with integers, should stay STYPE_INT
    # base_score interacts only with integers, should stay STYPE_INT




    
    final_score = base_score + 50

    # multiplier interacts with a float, should be refined to STYPE_FLOAT
    bonus = multiplier * 1.25

    # username interacts with a string, should be refined to STYPE_STRING
    greeting = username + " logged in."

    # unused_param is never used, should remain STYPE_INT (the default)

    print(final_score)
    print(bonus)
    print(greeting)

test_inference(100, 2.0, "admin", 0)
