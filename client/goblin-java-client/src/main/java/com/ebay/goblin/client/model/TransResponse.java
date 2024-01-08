package com.ebay.goblin.client.model;

import com.ebay.goblin.client.model.common.AbstractResult;
import lombok.Getter;
import lombok.Setter;
import lombok.ToString;

import java.util.ArrayList;
import java.util.List;

@Getter
@Setter
@ToString(callSuper = true)
public class TransResponse extends AbstractResult {

    public TransResponse() {
        results = new ArrayList<>();
        ignoredKeys = new ArrayList<>();
    }

    public void addResult(AbstractResult result) {
        results.add(result);
    }

    private final List<AbstractResult> results;

    private final List<String> ignoredKeys;
}
