package com.ebay.goblin.client.model;

import com.ebay.goblin.client.model.common.AbstractEntry;
import com.ebay.goblin.client.model.common.Condition;
import lombok.Getter;
import lombok.Setter;
import lombok.ToString;

import java.util.ArrayList;
import java.util.List;

@Getter
@Setter
@ToString(callSuper = true)
public class TransRequest extends AbstractEntry {

    public TransRequest() {
        preconds = new ArrayList<>();
        requests = new ArrayList<>();
        allowStale = false;
    }

    public void addPrecond(Condition cond) {
        preconds.add(cond);
    }

    public void addRequest(AbstractEntry req) {
        requests.add(req);
    }

    public List<Condition> getAllPreconds() {
        return preconds;
    }

    public List<AbstractEntry> getAllRequests() {
        return requests;
    }

    private final List<Condition> preconds;
    private final List<AbstractEntry> requests;
    private boolean allowStale;
}
