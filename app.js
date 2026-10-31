const form = document.getElementById("customerForm");
const message = document.getElementById("message");
const billResult = document.getElementById("billResult");
const customerTable = document.getElementById("customerTable");
const serverStatus = document.getElementById("serverStatus");
const printButton = document.getElementById("printButton");

const money = amount => "₹" + Number(amount || 0).toFixed(2);
const displayDate = date => date || "Not available";

async function apiRequest(url, options = {}) {
    const response = await fetch(url, options);
    let data;
    try {
        data = await response.json();
    } catch {
        throw new Error("Server returned an invalid response.");
    }
    if (!response.ok) throw new Error(data.error || "Request failed.");
    return data;
}

function postJSON(url, data) {
    return apiRequest(url, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(data)
    });
}

function showMessage(text, isError = false) {
    message.textContent = text;
    message.style.color = isError ? "#b91c1c" : "#13795b";
}

function addBillLine(label, value, parent = billResult) {
    const row = document.createElement("div");
    row.className = "bill-line";
    const left = document.createElement("span");
    left.textContent = label;
    const right = document.createElement("strong");
    right.textContent = value;
    row.append(left, right);
    parent.appendChild(row);
}

function showBill(customer) {
    billResult.replaceChildren();
    const heading = document.createElement("h3");
    heading.textContent = "Electricity Bill";
    billResult.appendChild(heading);

    const bill = Number(customer.bill || 0);
    const penalty = customer.paid ? 0 : Number(customer.penalty || 0);
    const totalPayable = customer.paid ? 0 :
        Number(customer.totalPayable ?? (bill + penalty));

    addBillLine("Customer ID", customer.id);
    addBillLine("Customer Name", customer.name);
    addBillLine("Units Consumed", customer.units);
    addBillLine("Bill Generation Date", displayDate(customer.generationDate));
    addBillLine("Due Date", displayDate(customer.dueDate));
    addBillLine("Energy Charges", money(bill - 50));
    addBillLine("Fixed Charge", money(50));
    addBillLine("Original Bill Amount", money(bill));
    addBillLine("Overdue Days", customer.generationDate ?
        (customer.overdueDays || 0) : "Date unavailable");
    addBillLine("Late Payment Penalty", money(penalty));
    addBillLine("Payment Status", customer.paid ? "PAID" : "UNPAID");

    const total = document.createElement("p");
    total.className = "bill-total";
    total.textContent = customer.paid ?
        "Outstanding: ₹0.00 (Paid)" :
        "Total Payable: " + money(totalPayable);
    billResult.appendChild(total);

    if (!customer.generationDate) {
        const note = document.createElement("p");
        note.textContent = "Older record: generation date unavailable, so penalty has not been calculated.";
        billResult.appendChild(note);
    }
    printButton.hidden = false;
}

function addCell(row, value) {
    const cell = document.createElement("td");
    cell.textContent = value;
    row.appendChild(cell);
    return cell;
}

async function loadCustomers() {
    try {
        const customers = await apiRequest("/api/customers");
        customerTable.replaceChildren();

        if (!Array.isArray(customers) || customers.length === 0) {
            const row = document.createElement("tr");
            const cell = addCell(row, "No customer records yet.");
            cell.colSpan = 10;
            customerTable.appendChild(row);
            return;
        }

        customers.forEach(customer => {
            const row = document.createElement("tr");
            addCell(row, customer.id);
            addCell(row, customer.name);
            addCell(row, customer.units);
            addCell(row, money(customer.bill));
            addCell(row, displayDate(customer.generationDate));
            addCell(row, displayDate(customer.dueDate));
            addCell(row, money(customer.paid ? 0 : customer.penalty));
            addCell(row, money(customer.paid ? 0 :
                (customer.totalPayable ?? (Number(customer.bill) + Number(customer.penalty || 0)))));

            const statusCell = addCell(row, customer.paid ? "PAID" : "UNPAID");
            statusCell.className = customer.paid ? "paid" : "unpaid";

            const actionCell = document.createElement("td");
            const editButton = document.createElement("button");
            editButton.className = "secondary small-button";
            editButton.textContent = "Edit";

            editButton.addEventListener("click", async () => {
                const newName = prompt("Enter customer name:", customer.name);
                if (newName === null) return;
                const newUnits = prompt("Enter electricity units:", customer.units);
                if (newUnits === null) return;

                const units = Number(newUnits);
                if (!newName.trim() || newUnits.trim() === "" ||
                    !Number.isInteger(units) || units < 0 || units > 1000000) {
                    alert("Enter a valid name and units (0 to 1000000).");
                    return;
                }

                try {
                    await postJSON("/api/edit", {
                        id: customer.id, name: newName.trim(), units
                    });
                    showMessage("Customer details updated successfully.");
                    await loadCustomers();
                } catch (error) {
                    showMessage(error.message, true);
                }
            });
            actionCell.appendChild(editButton);

            if (!customer.paid) {
                const payButton = document.createElement("button");
                payButton.className = "primary small-button";
                payButton.textContent = "Mark Paid";
                payButton.addEventListener("click", async () => {
                    const payable = Number(customer.totalPayable ??
                        (Number(customer.bill) + Number(customer.penalty || 0)));
                    if (!confirm("Mark this bill as paid? Total payable: " + money(payable))) return;
                    try {
                        await postJSON("/api/pay", { id: customer.id });
                        showMessage("Payment status updated.");
                        await loadCustomers();
                    } catch (error) {
                        showMessage(error.message, true);
                    }
                });
                actionCell.appendChild(payButton);
            } else {
                const completed = document.createElement("span");
                completed.textContent = "Completed";
                actionCell.appendChild(completed);
            }

            row.appendChild(actionCell);
            customerTable.appendChild(row);
        });
    } catch (error) {
        customerTable.replaceChildren();
        const row = document.createElement("tr");
        const cell = addCell(row, error.message);
        cell.colSpan = 10;
        customerTable.appendChild(row);
    }
}

form.addEventListener("submit", async event => {
    event.preventDefault();
    const customer = {
        id: Number(document.getElementById("customerId").value),
        name: document.getElementById("customerName").value.trim(),
        units: Number(document.getElementById("units").value)
    };

    try {
        const result = await postJSON("/api/customers", customer);
        showBill(result);
        showMessage("Bill generated and record saved!");
        form.reset();
        await loadCustomers();
    } catch (error) {
        showMessage(error.message, true);
    }
});

document.getElementById("searchButton").addEventListener("click", async () => {
    const id = Number(document.getElementById("searchId").value);
    if (!Number.isSafeInteger(id) || id <= 0) {
        showMessage("Enter a valid customer ID.", true);
        return;
    }
    try {
        const customer = await postJSON("/api/search", { id });
        showBill(customer);
        showMessage("Customer bill found.");
        document.getElementById("searchResult").textContent = "";
    } catch (error) {
        document.getElementById("searchResult").textContent = error.message;
        showMessage(error.message, true);
    }
});

document.getElementById("refreshButton").addEventListener("click", loadCustomers);
printButton.addEventListener("click", () => window.print());

async function checkServer() {
    try {
        await apiRequest("/api/health");
        serverStatus.textContent = "● Backend Connected";
    } catch {
        serverStatus.textContent = "● Backend Disconnected";
    }
}

checkServer();
loadCustomers();